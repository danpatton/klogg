#include "textsearchesdialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "configuration.h"

namespace {

// Build the "(regex) (ignore-case) (invert-match)" annotation shown in the
// rightmost column. Only flags that are set get a token; an unflagged search
// renders an empty cell. Same string is reused by the dropdown delegate.
QString flagAnnotations( const SavedSearch& s )
{
    QStringList tokens;
    if ( s.isRegex ) {
        tokens << QStringLiteral( "(regex)" );
    }
    if ( s.ignoreCase ) {
        tokens << QStringLiteral( "(ignore-case)" );
    }
    if ( s.invertMatch ) {
        tokens << QStringLiteral( "(invert-match)" );
    }
    return tokens.join( QLatin1Char( ' ' ) );
}

// Display string for the name column. Empty user-entered names render as a
// greyed-out "Unnamed" placeholder; the row's underlying SavedSearch keeps
// the empty name verbatim.
QString displayName( const SavedSearch& s )
{
    return s.name.isEmpty() ? QStringLiteral( "Unnamed" ) : s.name;
}

} // namespace

TextSearchesDialog::TextSearchesDialog( QWidget* parent )
    : QDialog( parent )
{
    setWindowTitle( "Text Searches" );
    buildUi();
    loadFromStorage();
    if ( !searches_.isEmpty() ) {
        table_->selectRow( 0 );
    }
    refreshButtons();
    refreshFields();
}

void TextSearchesDialog::buildUi()
{
    table_ = new QTableWidget( 0, 3, this );
    table_->setHorizontalHeaderLabels( { "Name", "Pattern", "Flags" } );
    table_->horizontalHeader()->setVisible( false );
    table_->verticalHeader()->setVisible( false );
    table_->setSelectionBehavior( QAbstractItemView::SelectRows );
    table_->setSelectionMode( QAbstractItemView::SingleSelection );
    table_->setEditTriggers( QAbstractItemView::NoEditTriggers );
    table_->setShowGrid( false );
    table_->setColumnWidth( 0, 160 );
    table_->horizontalHeader()->setStretchLastSection( true );
    table_->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
    connect( table_, &QTableWidget::currentCellChanged, this,
             [ this ]( int row, int, int, int ) { onCurrentRowChanged( row ); } );

    addBtn_ = new QPushButton( "Add", this );
    deleteBtn_ = new QPushButton( "Delete", this );
    moveUpBtn_ = new QPushButton( "Move Up", this );
    moveDownBtn_ = new QPushButton( "Move Down", this );
    connect( addBtn_, &QPushButton::clicked, this, &TextSearchesDialog::onAdd );
    connect( deleteBtn_, &QPushButton::clicked, this, &TextSearchesDialog::onDelete );
    connect( moveUpBtn_, &QPushButton::clicked, this, &TextSearchesDialog::onMoveUp );
    connect( moveDownBtn_, &QPushButton::clicked, this, &TextSearchesDialog::onMoveDown );

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget( addBtn_ );
    buttonRow->addWidget( deleteBtn_ );
    buttonRow->addStretch();
    buttonRow->addWidget( moveUpBtn_ );
    buttonRow->addWidget( moveDownBtn_ );

    nameEdit_ = new QLineEdit( this );
    connect( nameEdit_, &QLineEdit::textEdited, this, &TextSearchesDialog::onNameEdited );

    patternEdit_ = new QLineEdit( this );
    patternEdit_->setFont( Configuration::get().mainFont() );
    connect( patternEdit_, &QLineEdit::textEdited, this, &TextSearchesDialog::onPatternEdited );

    regexCheck_ = new QCheckBox( "&Regex Syntax", this );
    ignoreCaseCheck_ = new QCheckBox( "&Ignore Case", this );
    invertMatchCheck_ = new QCheckBox( "In&vert Match", this );
    connect( regexCheck_, &QCheckBox::toggled, this, &TextSearchesDialog::onRegexToggled );
    connect( ignoreCaseCheck_, &QCheckBox::toggled, this,
             &TextSearchesDialog::onIgnoreCaseToggled );
    connect( invertMatchCheck_, &QCheckBox::toggled, this,
             &TextSearchesDialog::onInvertMatchToggled );

    auto* form = new QGridLayout;
    form->addWidget( new QLabel( "Name:", this ), 0, 0 );
    form->addWidget( nameEdit_, 0, 1, 1, 4 );

    auto* stringHeaderRow = new QHBoxLayout;
    stringHeaderRow->addWidget( new QLabel( "String:", this ) );
    stringHeaderRow->addStretch();
    stringHeaderRow->addWidget( regexCheck_ );
    stringHeaderRow->addWidget( ignoreCaseCheck_ );
    stringHeaderRow->addWidget( invertMatchCheck_ );

    auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    connect( buttons, &QDialogButtonBox::accepted, this, &TextSearchesDialog::onAccept );
    connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

    auto* layout = new QVBoxLayout( this );
    layout->addWidget( table_, 1 );
    layout->addLayout( buttonRow );
    layout->addLayout( form );
    layout->addLayout( stringHeaderRow );
    layout->addWidget( patternEdit_ );
    layout->addWidget( buttons );

    resize( 640, 520 );
}

void TextSearchesDialog::loadFromStorage()
{
    searches_ = SavedSearches::get().items();
    table_->setRowCount( searches_.size() );
    for ( int i = 0; i < searches_.size(); ++i ) {
        refreshRow( i );
    }
}

void TextSearchesDialog::refreshRow( int row )
{
    const SavedSearch& s = searches_.at( row );

    // Match what the dropdown delegate paints: greyed "Unnamed" for empty,
    // monospace pattern, greyed flag annotations right-aligned.
    const QColor disabledFg
        = QApplication::palette().color( QPalette::Disabled, QPalette::Text );

    auto* nameItem = new QTableWidgetItem( displayName( s ) );
    if ( s.name.isEmpty() ) {
        nameItem->setForeground( disabledFg );
    }

    auto* patternItem = new QTableWidgetItem( s.pattern );
    patternItem->setFont( Configuration::get().mainFont() );

    auto* flagsItem = new QTableWidgetItem( flagAnnotations( s ) );
    flagsItem->setForeground( disabledFg );
    flagsItem->setTextAlignment( Qt::AlignRight | Qt::AlignVCenter );

    table_->setItem( row, 0, nameItem );
    table_->setItem( row, 1, patternItem );
    table_->setItem( row, 2, flagsItem );
}

int TextSearchesDialog::currentRow() const
{
    return table_->currentRow();
}

void TextSearchesDialog::refreshFields()
{
    const int row = currentRow();
    const bool hasRow = row >= 0 && row < searches_.size();

    // Block signals so updating the fields doesn't bounce back through the
    // *Edited / *Toggled slots and mutate the row we're trying to display.
    const QSignalBlocker name( nameEdit_ );
    const QSignalBlocker pattern( patternEdit_ );
    const QSignalBlocker regex( regexCheck_ );
    const QSignalBlocker ic( ignoreCaseCheck_ );
    const QSignalBlocker invert( invertMatchCheck_ );

    if ( hasRow ) {
        const SavedSearch& s = searches_.at( row );
        nameEdit_->setText( s.name );
        patternEdit_->setText( s.pattern );
        regexCheck_->setChecked( s.isRegex );
        ignoreCaseCheck_->setChecked( s.ignoreCase );
        invertMatchCheck_->setChecked( s.invertMatch );
    }
    else {
        nameEdit_->clear();
        patternEdit_->clear();
        regexCheck_->setChecked( false );
        ignoreCaseCheck_->setChecked( false );
        invertMatchCheck_->setChecked( false );
    }

    nameEdit_->setEnabled( hasRow );
    patternEdit_->setEnabled( hasRow );
    regexCheck_->setEnabled( hasRow );
    ignoreCaseCheck_->setEnabled( hasRow );
    invertMatchCheck_->setEnabled( hasRow );
}

void TextSearchesDialog::refreshButtons()
{
    const int row = currentRow();
    const bool hasRow = row >= 0;
    deleteBtn_->setEnabled( hasRow );
    moveUpBtn_->setEnabled( hasRow && row > 0 );
    moveDownBtn_->setEnabled( hasRow && row < searches_.size() - 1 );
}

void TextSearchesDialog::onCurrentRowChanged( int /*row*/ )
{
    refreshFields();
    refreshButtons();
}

void TextSearchesDialog::onAdd()
{
    searches_.append( SavedSearch{} );
    const int newRow = searches_.size() - 1;
    table_->insertRow( newRow );
    refreshRow( newRow );
    table_->selectRow( newRow );
    nameEdit_->setFocus();
}

void TextSearchesDialog::onDelete()
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    searches_.removeAt( row );
    table_->removeRow( row );
    const int next = std::min( row, searches_.size() - 1 );
    if ( next >= 0 ) {
        table_->selectRow( next );
    }
    else {
        refreshFields();
        refreshButtons();
    }
}

void TextSearchesDialog::onMoveUp()
{
    const int row = currentRow();
    if ( row <= 0 ) {
        return;
    }
    searches_.move( row, row - 1 );
    refreshRow( row );
    refreshRow( row - 1 );
    table_->selectRow( row - 1 );
}

void TextSearchesDialog::onMoveDown()
{
    const int row = currentRow();
    if ( row < 0 || row >= searches_.size() - 1 ) {
        return;
    }
    searches_.move( row, row + 1 );
    refreshRow( row );
    refreshRow( row + 1 );
    table_->selectRow( row + 1 );
}

void TextSearchesDialog::onNameEdited( const QString& text )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    searches_[ row ].name = text;
    refreshRow( row );
}

void TextSearchesDialog::onPatternEdited( const QString& text )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    searches_[ row ].pattern = text;
    refreshRow( row );
}

void TextSearchesDialog::onRegexToggled( bool checked )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    searches_[ row ].isRegex = checked;
    refreshRow( row );
}

void TextSearchesDialog::onIgnoreCaseToggled( bool checked )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    searches_[ row ].ignoreCase = checked;
    refreshRow( row );
}

void TextSearchesDialog::onInvertMatchToggled( bool checked )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    searches_[ row ].invertMatch = checked;
    refreshRow( row );
}

void TextSearchesDialog::onAccept()
{
    SavedSearches::get().setItems( searches_ );
    SavedSearches::get().save();
    Q_EMIT searchesChanged();
    accept();
}
