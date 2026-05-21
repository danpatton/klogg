#include "searchpane.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "iconloader.h"
#include "savedsearches.h"
#include "savedsearchespopup.h"

namespace {
// Toolbar-style flat button: no chrome at rest, raised border on hover,
// icon + text laid out horizontally. Matches the look of the QActions in
// the main toolbar.
QToolButton* makeToolButton( QWidget* parent, const QString& text )
{
    auto* button = new QToolButton( parent );
    button->setText( text );
    button->setAutoRaise( true );
    button->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );
    return button;
}
}

namespace {
// Delay between the last keystroke in the search box and the search firing.
// Short enough that interactive feedback feels live; long enough that we
// don't spawn a worker on every character of a fast typist.
constexpr int kDebounceMs = 200;
}

SearchPane::SearchPane( QWidget* parent )
    : QWidget( parent )
    , input_( new QLineEdit( this ) )
    , dropdownBtn_( new QToolButton( this ) )
    , addBtn_( makeToolButton( this, "Add" ) )
    , searchBtn_( makeToolButton( this, "Search" ) )
    , stopBtn_( makeToolButton( this, "Stop" ) )
    , clearBtn_( makeToolButton( this, "Clear" ) )
    , regexCheck_( new QCheckBox( "Regex Syntax", this ) )
    , ignoreCaseCheck_( new QCheckBox( "Ignore Case", this ) )
    , invertMatchCheck_( new QCheckBox( "Invert Match", this ) )
    , filterTailCheck_( new QCheckBox( "Filter Tail", this ) )
    , statusLabel_( new QLabel( this ) )
    , resultList_( new QTreeWidget( this ) )
    , debounce_( new QTimer( this ) )
    , savedSearchesPopup_( new SavedSearchesPopup( this ) )
{
    input_->setPlaceholderText( "Search..." );
    input_->setClearButtonEnabled( true );
    connect( input_, &QLineEdit::textChanged, this, &SearchPane::onSearchTextChanged );
    connect( input_, &QLineEdit::returnPressed, this, &SearchPane::onSearchClicked );

    dropdownBtn_->setAutoRaise( true );
    dropdownBtn_->setArrowType( Qt::DownArrow );
    dropdownBtn_->setToolTip( "Saved searches" );
    connect( dropdownBtn_, &QToolButton::clicked, this, &SearchPane::onDropdownClicked );
    connect( savedSearchesPopup_, &SavedSearchesPopup::searchSelected, this,
             &SearchPane::onSavedSearchSelected );

    addBtn_->setToolTip( "Save current search" );
    addBtn_->setEnabled( false );
    connect( addBtn_, &QToolButton::clicked, this, &SearchPane::onAddClicked );

    connect( searchBtn_, &QToolButton::clicked, this, &SearchPane::onSearchClicked );
    connect( stopBtn_, &QToolButton::clicked, this, &SearchPane::onStopClicked );
    connect( clearBtn_, &QToolButton::clicked, this, &SearchPane::onClearClicked );
    connect( filterTailCheck_, &QCheckBox::toggled, this, &SearchPane::filterTailToggled );

    // Toggling any of these mid-search changes the meaning of the current
    // pattern, so restart immediately rather than waiting for the user to
    // re-press Search.
    const auto restartOnToggle = [ this ]( bool ) { onSearchClicked(); };
    connect( regexCheck_, &QCheckBox::toggled, this, restartOnToggle );
    connect( ignoreCaseCheck_, &QCheckBox::toggled, this, restartOnToggle );
    connect( invertMatchCheck_, &QCheckBox::toggled, this, restartOnToggle );

    debounce_->setSingleShot( true );
    debounce_->setInterval( kDebounceMs );
    connect( debounce_, &QTimer::timeout, this, &SearchPane::onDebounceFired );

    resultList_->setRootIsDecorated( false );
    resultList_->setUniformRowHeights( true );
    resultList_->setSelectionMode( QAbstractItemView::SingleSelection );
    configureColumns( 0 );
    connect( resultList_, &QTreeWidget::itemActivated, this, &SearchPane::onResultActivated );
    // Single-click jump matches the BareTailPro feel; itemActivated covers
    // Enter / double-click for keyboard navigation.
    connect( resultList_, &QTreeWidget::itemClicked, this, &SearchPane::onResultActivated );

    IconLoader iconLoader( this );
    searchBtn_->setIcon( iconLoader.load( "baretail/search" ) );
    stopBtn_->setIcon( iconLoader.load( "baretail/stop" ) );
    clearBtn_->setIcon( iconLoader.load( "baretail/clear" ) );
    addBtn_->setIcon( iconLoader.load( "baretail/add" ) );

    auto* searchLabelIcon = new QLabel( this );
    searchLabelIcon->setPixmap( iconLoader.load( "baretail/regex" ).pixmap( 16, 16 ) );

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins( 4, 0, 4, 0 );
    topRow->setSpacing( 4 );
    topRow->addWidget( searchLabelIcon );
    topRow->addWidget( new QLabel( "Text", this ) );
    topRow->addWidget( input_, 1 );
    topRow->addWidget( dropdownBtn_ );
    topRow->addWidget( addBtn_ );
    topRow->addWidget( regexCheck_ );
    topRow->addWidget( ignoreCaseCheck_ );
    topRow->addWidget( invertMatchCheck_ );

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins( 4, 0, 4, 0 );
    btnRow->setSpacing( 4 );
    btnRow->addWidget( searchBtn_ );
    btnRow->addWidget( stopBtn_ );
    btnRow->addWidget( clearBtn_ );
    btnRow->addWidget( filterTailCheck_ );
    btnRow->addWidget( statusLabel_, 1 );

    auto* layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );
    layout->addLayout( topRow );
    layout->addLayout( btnRow );
    layout->addWidget( resultList_, 1 );
}

void SearchPane::focusSearchInput()
{
    input_->setFocus();
    input_->selectAll();
}

bool SearchPane::isFilterTailEnabled() const
{
    return filterTailCheck_->isChecked();
}

void SearchPane::applyMainFont( const QFont& font )
{
    input_->setFont( font );
    resultList_->setFont( font );
}

void SearchPane::onSearchTextChanged( const QString& text )
{
    addBtn_->setEnabled( !text.isEmpty() );
    if ( text.isEmpty() ) {
        // Empty input is treated as "no active search" — kill the pending
        // debounce so we don't fire an empty search immediately after the
        // user hits Clear or backspaces the last character.
        debounce_->stop();
        return;
    }
    debounce_->start();
}

void SearchPane::onDebounceFired()
{
    emitSearchRequest();
}

void SearchPane::onSearchClicked()
{
    debounce_->stop();
    emitSearchRequest();
}

void SearchPane::onStopClicked()
{
    debounce_->stop();
    Q_EMIT stopRequested();
}

void SearchPane::onClearClicked()
{
    debounce_->stop();
    input_->clear();
    clearResults();
    Q_EMIT clearRequested();
}

void SearchPane::onAddClicked()
{
    // Append the current pattern + flags as a new saved entry. Name is left
    // empty (renders as "Unnamed") — matches TextSearchesDialog::onAdd; the
    // user can rename via Preferences → Text Searches.
    const QString pattern = input_->text();
    if ( pattern.isEmpty() ) {
        return;
    }
    auto items = SavedSearches::get().items();
    items.append( SavedSearch{ QString(), pattern, regexCheck_->isChecked(),
                               ignoreCaseCheck_->isChecked(), invertMatchCheck_->isChecked() } );
    SavedSearches::get().setItems( std::move( items ) );
    SavedSearches::get().save();
}

void SearchPane::emitSearchRequest()
{
    const QString text = input_->text();
    if ( text.isEmpty() ) {
        return;
    }
    Q_EMIT searchRequested( text, regexCheck_->isChecked(), ignoreCaseCheck_->isChecked(),
                            invertMatchCheck_->isChecked() );
}

void SearchPane::onResultActivated()
{
    auto* item = resultList_->currentItem();
    if ( !item ) {
        return;
    }
    bool ok = false;
    const auto raw = item->data( 0, Qt::UserRole ).toULongLong( &ok );
    if ( !ok ) {
        return;
    }
    Q_EMIT jumpToLineRequested( LineNumber( raw ) );
}

void SearchPane::onDropdownClicked()
{
    // Re-pull from storage on every open so edits made via the Text
    // Searches dialog are reflected without us having to subscribe.
    savedSearchesPopup_->setItems( SavedSearches::get().items() );
    // Anchor under the input so the popup visually attaches to the field
    // rather than to the button. The input's bottom-left is also where
    // the user's eye is — feels less like a context menu, more like a
    // history dropdown.
    savedSearchesPopup_->popupBelow( input_ );
}

void SearchPane::onSavedSearchSelected( const SavedSearch& search )
{
    // Apply the saved entry as one atomic action: change all four widgets
    // with signals blocked so we don't fire a search per checkbox toggle
    // (the option-toggle restart connections would otherwise cascade).
    {
        const QSignalBlocker blockRegex( regexCheck_ );
        const QSignalBlocker blockIc( ignoreCaseCheck_ );
        const QSignalBlocker blockInvert( invertMatchCheck_ );
        const QSignalBlocker blockInput( input_ );
        regexCheck_->setChecked( search.isRegex );
        ignoreCaseCheck_->setChecked( search.ignoreCase );
        invertMatchCheck_->setChecked( search.invertMatch );
        input_->setText( search.pattern );
    }
    debounce_->stop();
    emitSearchRequest();
}

void SearchPane::appendResult( LineNumber line, const QString& text,
                               const QStringList& groupCaptures )
{
    auto* item = new QTreeWidgetItem( resultList_ );
    // Display 1-based line numbers — internally LineNumber is 0-based.
    item->setText( 0, QString::number( line.get() + 1 ) );
    item->setText( 1, text );
    const int count = std::min( static_cast<int>( groupCaptures.size() ), captureGroupCount_ );
    for ( int i = 0; i < count; ++i ) {
        item->setText( 2 + i, groupCaptures.at( i ) );
    }
    item->setData( 0, Qt::UserRole, QVariant::fromValue( line.get() ) );
}

void SearchPane::configureColumns( int captureGroupCount )
{
    captureGroupCount_ = std::max( 0, captureGroupCount );
    QStringList headers{ "Line", "Text" };
    for ( int i = 1; i <= captureGroupCount_; ++i ) {
        headers << QString::number( i );
    }
    resultList_->setColumnCount( headers.size() );
    resultList_->setHeaderLabels( headers );

    auto* header = resultList_->header();
    header->resizeSection( 0, 80 );
    if ( captureGroupCount_ == 0 ) {
        // Text fills the remaining width.
        header->setStretchLastSection( true );
    }
    else {
        // Text stays dominant; group columns get an interactive default
        // width so users can resize them.
        header->setStretchLastSection( false );
        header->setSectionResizeMode( 1, QHeaderView::Stretch );
        for ( int i = 0; i < captureGroupCount_; ++i ) {
            header->resizeSection( 2 + i, 120 );
        }
    }
}

void SearchPane::clearResults()
{
    resultList_->clear();
    statusLabel_->clear();
}

void SearchPane::setStatusText( const QString& text )
{
    statusLabel_->setText( text );
}
