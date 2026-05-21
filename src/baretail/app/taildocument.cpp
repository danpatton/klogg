#include "taildocument.h"

#include <QScrollBar>
#include <QVBoxLayout>

#include "abstractlogview.h"
#include "configuration.h"
#include "logdata.h"
#include "logfiltereddata.h"
#include "logmainview.h"
#include "quickfindpattern.h"
#include "searchpane.h"

TailDocument::TailDocument( const QString& fileName, QWidget* parent )
    : QWidget( parent )
    , fileName_( fileName )
    , logData_( std::make_unique<LogData>() )
    , filteredData_( logData_->getNewFilteredData() )
    , qfp_( std::make_unique<QuickFindPattern>() )
    , view_( new LogMainView( logData_.get(), qfp_.get(), nullptr, nullptr, this ) )
    , searchPane_( new SearchPane( this ) )
{
    // useNewFiltering wires the marks-bullet column and gives LogMainView's
    // own [/] shortcuts a non-null LogFilteredData to dereference.
    view_->useNewFiltering( filteredData_.get() );

    // AbstractLogView's ctor never consults Configuration::mainFont(); it
    // uses Qt's inherited widget font. Apply the configured font here so
    // new tabs match what the user picked in Tools -> Font...
    const QFont mainFont = Configuration::get().mainFont();
    view_->updateFont( mainFont );
    searchPane_->applyMainFont( mainFont );

    connect( view_, &AbstractLogView::markLines,
             this, &TailDocument::onMarkLinesRequested );

    // AbstractLogView's mousePressEvent updates selection_ and emits newSelection
    // but deliberately does not call update() itself; klogg's CrawlerWidget wires
    // the repaint. Without this connection, only the first click visibly
    // updates (subsequent paints from elsewhere never come).
    connect( view_, &AbstractLogView::newSelection,
             view_, [ this ]( auto, auto, auto, auto ) { view_->update(); } );

    connect( searchPane_, &SearchPane::searchRequested,
             this, &TailDocument::onSearchRequested );
    connect( searchPane_, &SearchPane::stopRequested,
             this, &TailDocument::onStopRequested );
    connect( searchPane_, &SearchPane::clearRequested,
             this, &TailDocument::onClearRequested );
    connect( searchPane_, &SearchPane::jumpToLineRequested,
             this, &TailDocument::onJumpToLineRequested );

    connect( filteredData_.get(), &LogFilteredData::searchProgressed,
             this, &TailDocument::onSearchProgressed );

    auto* layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );
    layout->addWidget( view_, 3 );
    layout->addWidget( searchPane_, 2 );

    connect( logData_.get(), &LogData::loadingFinished,
             this, &TailDocument::onLoadingFinished );

    view_->followSet( true );

    // Deliberately NOT calling view_->registerShortcuts(): klogg's chord set
    // (F3/N/Ctrl+G for find, [/] for marks, M for mark, vim-style J/K, etc.)
    // conflicts with our MainWindow-level shortcuts and isn't part of the
    // BareTailPro keyboard model anyway. Arrows / PgUp / PgDn / Home / End
    // still work via QAbstractScrollArea's default key handling.
    logData_->attachFile( fileName_ );
}

TailDocument::~TailDocument() = default;

LinesCount TailDocument::lineCount() const
{
    return logData_->getNbLine();
}

qint64 TailDocument::fileSize() const
{
    return logData_->getFileSize();
}

bool TailDocument::isFollowEnabled() const
{
    return view_->isFollowEnabled();
}

void TailDocument::setFollowEnabled( bool follow )
{
    view_->followSet( follow );
}

void TailDocument::refreshView()
{
    view_->forceRefresh();
}

void TailDocument::jumpToTop()
{
    view_->verticalScrollBar()->setValue( 0 );
}

void TailDocument::jumpToBottom()
{
    view_->verticalScrollBar()->setValue( view_->verticalScrollBar()->maximum() );
}

void TailDocument::applyFont( const QFont& font )
{
    view_->updateFont( font );
    searchPane_->applyMainFont( font );
}

void TailDocument::focusSearch()
{
    searchPane_->focusSearchInput();
}

void TailDocument::onSearchRequested( const QString& pattern, bool isRegex, bool ignoreCase,
                                      bool invertMatch )
{
    // Any in-flight search must be interrupted before we mutate the
    // pattern — runSearch() will otherwise block.
    filteredData_->interruptSearch();
    filteredData_->clearSearch();
    searchPane_->clearResults();
    resultsShown_ = 0;

    // Compile the capture-group regex up front. We use it both to extract
    // captures in appendNewMatches() and — more importantly — to validate
    // the pattern before handing it to the search worker. The worker's
    // own regex (inside RegularExpressionPattern) uses DontCaptureOption
    // for matching speed, so we keep a separate copy here without it.
    if ( isRegex ) {
        QRegularExpression::PatternOptions opts = QRegularExpression::UseUnicodePropertiesOption;
        if ( ignoreCase ) {
            opts |= QRegularExpression::CaseInsensitiveOption;
        }
        captureRegex_ = QRegularExpression( pattern, opts );
    }
    else {
        captureRegex_ = QRegularExpression();
    }

    if ( isRegex && !captureRegex_.isValid() ) {
        // Don't dispatch an invalid regex to the search worker. It would
        // call QRegularExpression::match() per line, and Qt logs
        // "QRegularExpressionPrivate::doMatch(): called on an invalid
        // QRegularExpression object" on every call. Surface the parse
        // error in the status row and wait for the user to finish typing.
        captureGroupCount_ = 0;
        searchPane_->configureColumns( 0 );
        searchActive_ = false;
        searchPane_->setStatusText(
            QString( "Invalid regex: %1" ).arg( captureRegex_.errorString() ) );
        return;
    }

    captureGroupCount_ = isRegex ? std::max( 0, captureRegex_.captureCount() ) : 0;
    searchPane_->configureColumns( captureGroupCount_ );

    currentPattern_ = RegularExpressionPattern( pattern, /*caseSensitive=*/!ignoreCase,
                                                /*inverse=*/invertMatch, /*boolean=*/false,
                                                /*plainText=*/!isRegex );
    searchActive_ = true;

    // "Searching..." must be set BEFORE runSearch: cache hits in
    // LogFilteredData emit searchProgressed synchronously with progress=100
    // from inside runSearch, and our onSearchProgressed handler writes the
    // final "Found N matching lines" status. If we set "Searching..." after
    // runSearch returns, it overwrites that final status and the UI is
    // stuck at "Searching..." forever for any previously-cached search.
    searchPane_->setStatusText( "Searching..." );

    filteredData_->runSearch( currentPattern_ );
}

void TailDocument::onStopRequested()
{
    filteredData_->interruptSearch();
    // Leave existing results visible — Stop is "pause", not "wipe".
    searchPane_->setStatusText(
        QString( "Stopped (%1 matches)" ).arg( filteredData_->getNbMatches().get() ) );
}

void TailDocument::onClearRequested()
{
    filteredData_->interruptSearch();
    filteredData_->clearSearch();
    searchActive_ = false;
    resultsShown_ = 0;
    view_->update();
}

void TailDocument::onJumpToLineRequested( LineNumber line )
{
    view_->selectAndDisplayLine( line );
    view_->setFocus();
}

void TailDocument::onSearchProgressed( LinesCount nbMatches, int progress,
                                       LineNumber /*initialLine*/ )
{
    appendNewMatches();
    if ( progress >= 100 ) {
        searchPane_->setStatusText(
            QString( "Found %1 matching lines" ).arg( nbMatches.get() ) );
    }
    else {
        searchPane_->setStatusText(
            QString( "Found %1 matching lines so far (%2%)" )
                .arg( nbMatches.get() )
                .arg( progress ) );
    }
}

void TailDocument::appendNewMatches()
{
    const auto total = filteredData_->getNbMatches().get();
    for ( quint64 i = resultsShown_; i < total; ++i ) {
        const auto matchIndex = LineNumber( i );
        const auto sourceLine = filteredData_->getMatchingLineNumber( matchIndex );
        const QString text = logData_->getLineString( sourceLine );
        QStringList groups;
        if ( captureGroupCount_ > 0 ) {
            const auto m = captureRegex_.match( text );
            if ( m.hasMatch() ) {
                groups.reserve( captureGroupCount_ );
                for ( int g = 1; g <= captureGroupCount_; ++g ) {
                    groups << m.captured( g );
                }
            }
        }
        searchPane_->appendResult( sourceLine, text, groups );
    }
    resultsShown_ = total;
}

void TailDocument::onMarkLinesRequested( const klogg::vector<LineNumber>& lines )
{
    for ( const auto& line : lines ) {
        filteredData_->toggleMark( line );
    }
    view_->forceRefresh();
}

void TailDocument::toggleBookmark()
{
    filteredData_->toggleMark( view_->currentLine() );
    view_->forceRefresh();
}

void TailDocument::nextBookmark()
{
    const auto next = filteredData_->getMarkAfter( view_->currentLine() );
    if ( next.has_value() ) {
        view_->selectAndDisplayLine( *next );
    }
}

void TailDocument::prevBookmark()
{
    const auto prev = filteredData_->getMarkBefore( view_->currentLine() );
    if ( prev.has_value() ) {
        view_->selectAndDisplayLine( *prev );
    }
}

void TailDocument::clearBookmarks()
{
    filteredData_->clearMarks();
    view_->forceRefresh();
}

void TailDocument::onLoadingFinished( LoadingStatus status )
{
    if ( status != LoadingStatus::Successful ) {
        return;
    }
    view_->updateData();

    // AbstractLogView gates its highlighter pass on (searchStart_, searchEnd_).
    // Those default to (0, getNbLine()-at-construction-time) which is (0, 0)
    // because we attachFile after constructing the view. Without this push
    // every line falls outside the range and gets the disabled-text colour
    // instead of the highlighter's colour. baretail has no search-limits
    // feature, so just track the data extent.
    const auto totalLines = logData_->getNbLine();
    view_->setSearchLimits( 0_lnum, LineNumber( totalLines.get() ) );

    // Filter Tail: extend the active search over the newly indexed range.
    // updateSearch resumes from wherever the worker last finished.
    if ( searchActive_ && searchPane_->isFilterTailEnabled() ) {
        filteredData_->updateSearch( 0_lnum, LineNumber( totalLines.get() ) );
    }

    Q_EMIT linesUpdated( totalLines );
}
