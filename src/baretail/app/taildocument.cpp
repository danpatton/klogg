#include "taildocument.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

#include "abstractlogview.h"
#include "configuration.h"
#include "logdata.h"
#include "logfiltereddata.h"
#include "logmainview.h"
#include "quickfindpattern.h"

TailDocument::TailDocument( const QString& fileName, QWidget* parent )
    : QWidget( parent )
    , fileName_( fileName )
    , logData_( std::make_unique<LogData>() )
    , filteredData_( logData_->getNewFilteredData() )
    , qfp_( std::make_unique<QuickFindPattern>() )
    , view_( new LogMainView( logData_.get(), qfp_.get(), nullptr, nullptr, this ) )
{
    // useNewFiltering wires the marks-bullet column and gives LogMainView's
    // own [/] shortcuts a non-null LogFilteredData to dereference.
    view_->useNewFiltering( filteredData_.get() );

    // AbstractLogView's ctor never consults Configuration::mainFont(); it
    // uses Qt's inherited widget font. Apply the configured font here so
    // new tabs match what the user picked in Tools -> Font...
    view_->updateFont( Configuration::get().mainFont() );

    connect( view_, &AbstractLogView::markLines,
             this, &TailDocument::onMarkLinesRequested );

    // AbstractLogView's mousePressEvent updates selection_ and emits newSelection
    // but deliberately does not call update() itself; klogg's CrawlerWidget wires
    // the repaint. Without this connection, only the first click visibly
    // updates (subsequent paints from elsewhere never come).
    connect( view_, &AbstractLogView::newSelection,
             view_, [ this ]( auto, auto, auto, auto ) { view_->update(); } );

    auto* layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );
    layout->addWidget( view_ );

    buildFindBar();
    layout->addWidget( findBar_ );

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

void TailDocument::buildFindBar()
{
    findBar_ = new QWidget( this );
    findBar_->setVisible( false );

    auto* findLabel = new QLabel( "Find:", findBar_ );

    findInput_ = new QLineEdit( findBar_ );
    findInput_->setClearButtonEnabled( true );
    connect( findInput_, &QLineEdit::textChanged, this, &TailDocument::onFindTextChanged );
    connect( findInput_, &QLineEdit::returnPressed, this, &TailDocument::findNext );

    findPrevBtn_ = new QToolButton( findBar_ );
    findPrevBtn_->setText( "Prev" );
    findPrevBtn_->setToolTip( "Find previous (Shift+F3)" );
    connect( findPrevBtn_, &QToolButton::clicked, this, &TailDocument::findPrev );

    findNextBtn_ = new QToolButton( findBar_ );
    findNextBtn_->setText( "Next" );
    findNextBtn_->setToolTip( "Find next (F3)" );
    connect( findNextBtn_, &QToolButton::clicked, this, &TailDocument::findNext );

    findCloseBtn_ = new QToolButton( findBar_ );
    findCloseBtn_->setText( "×" );
    findCloseBtn_->setToolTip( "Close (Esc)" );
    connect( findCloseBtn_, &QToolButton::clicked, this, &TailDocument::hideFindBar );

    // Esc anywhere inside the find bar dismisses it.
    auto* hideShortcut = new QShortcut( QKeySequence( Qt::Key_Escape ), findBar_ );
    hideShortcut->setContext( Qt::WidgetWithChildrenShortcut );
    connect( hideShortcut, &QShortcut::activated, this, &TailDocument::hideFindBar );

    auto* layout = new QHBoxLayout( findBar_ );
    layout->setContentsMargins( 4, 2, 4, 2 );
    layout->addWidget( findLabel );
    layout->addWidget( findInput_, 1 );
    layout->addWidget( findPrevBtn_ );
    layout->addWidget( findNextBtn_ );
    layout->addWidget( findCloseBtn_ );
}

TailDocument::~TailDocument() = default;

LinesCount TailDocument::lineCount() const
{
    return logData_->getNbLine();
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
}

void TailDocument::showFindBar()
{
    findBar_->setVisible( true );
    findInput_->setFocus();
    findInput_->selectAll();
}

void TailDocument::hideFindBar()
{
    findBar_->setVisible( false );
    qfp_->changeSearchPattern( QString() );
    view_->setFocus();
}

void TailDocument::onFindTextChanged( const QString& text )
{
    // Plain-substring by default. The in-body highlight repaints automatically
    // because LogMainView is connected to QuickFindPattern::patternUpdated.
    qfp_->changeSearchPattern( text, /*isRegex=*/false );
}

void TailDocument::findNext()
{
    if ( qfp_->isActive() ) {
        view_->searchForward();
    }
}

void TailDocument::findPrev()
{
    if ( qfp_->isActive() ) {
        view_->searchBackward();
    }
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

    Q_EMIT linesUpdated( totalLines );
}
