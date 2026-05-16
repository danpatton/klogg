#include "highlightingdialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Render a colour swatch into the toolbutton icon so it shows the current
// fore/back colour the way BareTailPro's colour controls do.
QIcon swatchIcon( const QColor& color )
{
    QPixmap pix( 32, 16 );
    pix.fill( color.isValid() ? color : Qt::transparent );
    return QIcon( pix );
}

Highlighter makeDefaultRule()
{
    // Sensible defaults for a new row: black-on-yellow, ignore-case off, no
    // regex (plain substring). Matches what BareTailPro starts you with.
    Highlighter h( QString(), /*ignoreCase=*/false, /*onlyMatch=*/false,
                   QColor( Qt::black ), QColor( Qt::yellow ) );
    h.setUseRegex( false );
    return h;
}

} // namespace

HighlightingDialog::HighlightingDialog( const QString& setName, QWidget* parent )
    : QDialog( parent )
    , setName_( setName )
{
    setWindowTitle( "Highlighting" );
    buildUi();
    loadFromCollection();
    if ( !rules_.isEmpty() ) {
        table_->selectRow( 0 );
    }
    refreshButtons();
    refreshFields();
}

void HighlightingDialog::buildUi()
{
    table_ = new QTableWidget( 0, 2, this );
    table_->setHorizontalHeaderLabels( { "Preview", "Pattern" } );
    table_->horizontalHeader()->setStretchLastSection( true );
    table_->horizontalHeader()->setVisible( false );
    table_->verticalHeader()->setVisible( false );
    table_->setSelectionBehavior( QAbstractItemView::SelectRows );
    table_->setSelectionMode( QAbstractItemView::SingleSelection );
    table_->setEditTriggers( QAbstractItemView::NoEditTriggers );
    table_->setShowGrid( false );
    table_->setColumnWidth( 0, 100 );
    connect( table_, &QTableWidget::currentCellChanged, this,
             [ this ]( int row, int, int, int ) { onCurrentRowChanged( row ); } );

    addBtn_ = new QPushButton( "Add", this );
    deleteBtn_ = new QPushButton( "Delete", this );
    moveUpBtn_ = new QPushButton( "Move Up", this );
    moveDownBtn_ = new QPushButton( "Move Down", this );
    connect( addBtn_, &QPushButton::clicked, this, &HighlightingDialog::onAdd );
    connect( deleteBtn_, &QPushButton::clicked, this, &HighlightingDialog::onDelete );
    connect( moveUpBtn_, &QPushButton::clicked, this, &HighlightingDialog::onMoveUp );
    connect( moveDownBtn_, &QPushButton::clicked, this, &HighlightingDialog::onMoveDown );

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addWidget( addBtn_ );
    buttonRow->addWidget( deleteBtn_ );
    buttonRow->addStretch();
    buttonRow->addWidget( moveUpBtn_ );
    buttonRow->addWidget( moveDownBtn_ );

    foreColorBtn_ = new QToolButton( this );
    foreColorBtn_->setIconSize( QSize( 32, 16 ) );
    backColorBtn_ = new QToolButton( this );
    backColorBtn_->setIconSize( QSize( 32, 16 ) );
    connect( foreColorBtn_, &QToolButton::clicked, this, &HighlightingDialog::onPickForeColor );
    connect( backColorBtn_, &QToolButton::clicked, this, &HighlightingDialog::onPickBackColor );

    patternEdit_ = new QLineEdit( this );
    connect( patternEdit_, &QLineEdit::textEdited, this, &HighlightingDialog::onPatternEdited );

    ignoreCaseCheck_ = new QCheckBox( "Ignore Case", this );
    connect( ignoreCaseCheck_, &QCheckBox::toggled, this,
             &HighlightingDialog::onIgnoreCaseToggled );

    auto* form = new QGridLayout;
    form->addWidget( new QLabel( "Foreground Color:", this ), 0, 0 );
    form->addWidget( foreColorBtn_, 0, 1 );
    form->addWidget( new QLabel( "Background Color:", this ), 0, 2 );
    form->addWidget( backColorBtn_, 0, 3 );
    form->addWidget( new QLabel( "String:", this ), 1, 0 );
    form->addWidget( patternEdit_, 1, 1, 1, 3 );
    form->addWidget( ignoreCaseCheck_, 2, 0, 1, 4 );
    form->setColumnStretch( 1, 1 );
    form->setColumnStretch( 3, 1 );

    auto* buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );
    connect( buttons, &QDialogButtonBox::accepted, this, &HighlightingDialog::onAccept );
    connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

    auto* layout = new QVBoxLayout( this );
    layout->addWidget( table_, 1 );
    layout->addLayout( buttonRow );
    layout->addLayout( form );
    layout->addWidget( buttons );

    resize( 460, 480 );
}

void HighlightingDialog::loadFromCollection()
{
    const auto& sets = HighlighterSetCollection::get().highlighterSets();
    for ( const auto& set : sets ) {
        if ( set.name() == setName_ ) {
            rules_ = set.highlighters();
            break;
        }
    }
    table_->setRowCount( rules_.size() );
    for ( int i = 0; i < rules_.size(); ++i ) {
        refreshRow( i );
    }
}

void HighlightingDialog::refreshRow( int row )
{
    const Highlighter& h = rules_.at( row );
    auto* previewItem = new QTableWidgetItem( h.pattern() );
    previewItem->setForeground( h.foreColor() );
    previewItem->setBackground( h.backColor() );
    auto* patternItem = new QTableWidgetItem( h.pattern() );
    table_->setItem( row, 0, previewItem );
    table_->setItem( row, 1, patternItem );
}

int HighlightingDialog::currentRow() const
{
    return table_->currentRow();
}

void HighlightingDialog::refreshFields()
{
    const int row = currentRow();
    const bool hasRule = row >= 0 && row < rules_.size();

    // Block signals so updating the fields doesn't bounce back through the
    // *Edited slots and mutate the rule we're trying to display.
    const QSignalBlocker pattern( patternEdit_ );
    const QSignalBlocker ic( ignoreCaseCheck_ );

    if ( hasRule ) {
        const Highlighter& h = rules_.at( row );
        patternEdit_->setText( h.pattern() );
        ignoreCaseCheck_->setChecked( h.ignoreCase() );
        refreshColorButton( foreColorBtn_, h.foreColor() );
        refreshColorButton( backColorBtn_, h.backColor() );
    }
    else {
        patternEdit_->clear();
        ignoreCaseCheck_->setChecked( false );
        refreshColorButton( foreColorBtn_, {} );
        refreshColorButton( backColorBtn_, {} );
    }

    patternEdit_->setEnabled( hasRule );
    ignoreCaseCheck_->setEnabled( hasRule );
    foreColorBtn_->setEnabled( hasRule );
    backColorBtn_->setEnabled( hasRule );
}

void HighlightingDialog::refreshButtons()
{
    const int row = currentRow();
    const bool hasRule = row >= 0;
    deleteBtn_->setEnabled( hasRule );
    moveUpBtn_->setEnabled( hasRule && row > 0 );
    moveDownBtn_->setEnabled( hasRule && row < rules_.size() - 1 );
}

void HighlightingDialog::refreshColorButton( QToolButton* button, const QColor& color )
{
    button->setIcon( swatchIcon( color ) );
}

void HighlightingDialog::onCurrentRowChanged( int /*row*/ )
{
    refreshFields();
    refreshButtons();
}

void HighlightingDialog::onAdd()
{
    rules_.append( makeDefaultRule() );
    const int newRow = rules_.size() - 1;
    table_->insertRow( newRow );
    refreshRow( newRow );
    table_->selectRow( newRow );
    patternEdit_->setFocus();
}

void HighlightingDialog::onDelete()
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    rules_.removeAt( row );
    table_->removeRow( row );
    const int next = std::min( row, rules_.size() - 1 );
    if ( next >= 0 ) {
        table_->selectRow( next );
    }
    else {
        refreshFields();
        refreshButtons();
    }
}

void HighlightingDialog::onMoveUp()
{
    const int row = currentRow();
    if ( row <= 0 ) {
        return;
    }
    rules_.move( row, row - 1 );
    refreshRow( row );
    refreshRow( row - 1 );
    table_->selectRow( row - 1 );
}

void HighlightingDialog::onMoveDown()
{
    const int row = currentRow();
    if ( row < 0 || row >= rules_.size() - 1 ) {
        return;
    }
    rules_.move( row, row + 1 );
    refreshRow( row );
    refreshRow( row + 1 );
    table_->selectRow( row + 1 );
}

void HighlightingDialog::onPatternEdited( const QString& text )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    rules_[ row ].setPattern( text );
    refreshRow( row );
}

void HighlightingDialog::onIgnoreCaseToggled( bool checked )
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    rules_[ row ].setIgnoreCase( checked );
}

void HighlightingDialog::onPickForeColor()
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    const QColor chosen
        = QColorDialog::getColor( rules_[ row ].foreColor(), this, "Foreground Color" );
    if ( chosen.isValid() ) {
        rules_[ row ].setForeColor( chosen );
        refreshColorButton( foreColorBtn_, chosen );
        refreshRow( row );
    }
}

void HighlightingDialog::onPickBackColor()
{
    const int row = currentRow();
    if ( row < 0 ) {
        return;
    }
    const QColor chosen
        = QColorDialog::getColor( rules_[ row ].backColor(), this, "Background Color" );
    if ( chosen.isValid() ) {
        rules_[ row ].setBackColor( chosen );
        refreshColorButton( backColorBtn_, chosen );
        refreshRow( row );
    }
}

void HighlightingDialog::onAccept()
{
    auto& collection = HighlighterSetCollection::get();
    auto sets = collection.highlighterSets();
    for ( auto& set : sets ) {
        if ( set.name() == setName_ ) {
            set.highlighters() = rules_;
            break;
        }
    }
    collection.setHighlighterSets( sets );
    collection.save();
    Q_EMIT rulesChanged();
    accept();
}
