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
{
    input_->setPlaceholderText( "Search..." );
    input_->setClearButtonEnabled( true );
    connect( input_, &QLineEdit::textChanged, this, &SearchPane::onSearchTextChanged );
    connect( input_, &QLineEdit::returnPressed, this, &SearchPane::onSearchClicked );

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
    resultList_->setColumnCount( 2 );
    resultList_->setHeaderLabels( { "Line", "Text" } );
    resultList_->header()->setStretchLastSection( true );
    resultList_->header()->resizeSection( 0, 80 );
    connect( resultList_, &QTreeWidget::itemActivated, this, &SearchPane::onResultActivated );
    // Single-click jump matches the BareTailPro feel; itemActivated covers
    // Enter / double-click for keyboard navigation.
    connect( resultList_, &QTreeWidget::itemClicked, this, &SearchPane::onResultActivated );

    IconLoader iconLoader( this );
    searchBtn_->setIcon( iconLoader.load( "baretail/search" ) );
    stopBtn_->setIcon( iconLoader.load( "baretail/stop" ) );
    clearBtn_->setIcon( iconLoader.load( "baretail/clear" ) );

    auto* searchLabelIcon = new QLabel( this );
    searchLabelIcon->setPixmap( iconLoader.load( "baretail/regex" ).pixmap( 16, 16 ) );

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins( 4, 0, 4, 0 );
    topRow->setSpacing( 4 );
    topRow->addWidget( searchLabelIcon );
    topRow->addWidget( new QLabel( "Text", this ) );
    topRow->addWidget( input_, 1 );
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

void SearchPane::applyResultsFont( const QFont& font )
{
    resultList_->setFont( font );
}

void SearchPane::onSearchTextChanged( const QString& text )
{
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

void SearchPane::appendResult( LineNumber line, const QString& text )
{
    auto* item = new QTreeWidgetItem( resultList_ );
    // Display 1-based line numbers — internally LineNumber is 0-based.
    item->setText( 0, QString::number( line.get() + 1 ) );
    item->setText( 1, text );
    item->setData( 0, Qt::UserRole, QVariant::fromValue( line.get() ) );
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
