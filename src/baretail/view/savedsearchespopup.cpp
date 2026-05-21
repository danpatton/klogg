#include "savedsearchespopup.h"

#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QListView>
#include <QPainter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include "configuration.h"

namespace {

constexpr int kRoleSavedSearch = Qt::UserRole + 1;

// Build the right-aligned flag annotation string painted alongside each
// row. Empty when no flags are set. Kept in sync with the dialog so the
// dropdown and the dialog table read the same.
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

QString displayName( const SavedSearch& s )
{
    return s.name.isEmpty() ? QStringLiteral( "Unnamed" ) : s.name;
}

// Three-column row painter: name | pattern (monospace) | flags (greyed,
// right-aligned). Selected rows are filled with the highlight colour and
// all text rendered in highlightedText so the columns share one strong
// foreground rather than a mix of greyed and normal.
class SavedSearchDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint( QPainter* painter, const QStyleOptionViewItem& option,
                const QModelIndex& index ) const override
    {
        painter->save();

        const auto search = index.data( kRoleSavedSearch ).value<SavedSearch>();
        const bool selected = option.state & QStyle::State_Selected;
        const QPalette& pal = option.palette;

        if ( selected ) {
            painter->fillRect( option.rect, pal.color( QPalette::Highlight ) );
        }

        const QFont baseFont = option.font;
        const QFont patternFont = Configuration::get().mainFont();

        const QColor nameColor = selected
                                     ? pal.color( QPalette::HighlightedText )
                                     : ( search.name.isEmpty()
                                             ? pal.color( QPalette::Disabled, QPalette::Text )
                                             : pal.color( QPalette::Text ) );
        const QColor patternColor
            = selected ? pal.color( QPalette::HighlightedText ) : pal.color( QPalette::Text );
        const QColor flagColor
            = selected ? pal.color( QPalette::HighlightedText )
                       : pal.color( QPalette::Disabled, QPalette::Text );

        // Layout: fixed 30% for name, fixed 20% on the right for flags,
        // pattern stretches in the middle. Padding mirrors what Qt's
        // default delegate uses so adjacent table rows in the dialog and
        // the popup line up at the eye.
        constexpr int kPad = 6;
        QRect r = option.rect.adjusted( kPad, 0, -kPad, 0 );
        const int nameWidth = r.width() * 3 / 10;
        const int flagsWidth = r.width() * 2 / 10;

        const QRect nameRect( r.left(), r.top(), nameWidth, r.height() );
        const QRect flagsRect( r.right() - flagsWidth, r.top(), flagsWidth, r.height() );
        const QRect patternRect( r.left() + nameWidth + kPad, r.top(),
                                 r.width() - nameWidth - flagsWidth - 2 * kPad, r.height() );

        painter->setFont( baseFont );
        painter->setPen( nameColor );
        painter->drawText( nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                           painter->fontMetrics().elidedText(
                               displayName( search ), Qt::ElideRight, nameRect.width() ) );

        painter->setFont( patternFont );
        painter->setPen( patternColor );
        painter->drawText( patternRect, Qt::AlignLeft | Qt::AlignVCenter,
                           painter->fontMetrics().elidedText(
                               search.pattern, Qt::ElideRight, patternRect.width() ) );

        painter->setFont( baseFont );
        painter->setPen( flagColor );
        painter->drawText( flagsRect, Qt::AlignRight | Qt::AlignVCenter,
                           painter->fontMetrics().elidedText(
                               flagAnnotations( search ), Qt::ElideRight, flagsRect.width() ) );

        painter->restore();
    }

    QSize sizeHint( const QStyleOptionViewItem& option,
                    const QModelIndex& /*index*/ ) const override
    {
        const QFontMetrics fm( option.font );
        // +6 of vertical padding gives the row a comparable feel to the
        // dialog table without depending on style metrics.
        return QSize( 200, fm.height() + 6 );
    }
};

} // namespace

SavedSearchesPopup::SavedSearchesPopup( QWidget* parent )
    : QFrame( parent, Qt::Popup )
    , list_( new QListView( this ) )
    , model_( new QStandardItemModel( this ) )
{
    setFrameStyle( QFrame::Panel | QFrame::Raised );
    setFocusPolicy( Qt::StrongFocus );

    list_->setModel( model_ );
    list_->setItemDelegate( new SavedSearchDelegate( list_ ) );
    list_->setSelectionMode( QAbstractItemView::SingleSelection );
    list_->setEditTriggers( QAbstractItemView::NoEditTriggers );
    list_->setUniformItemSizes( true );
    list_->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    // Single-click matches the BareTailPro feel (cf. SearchPane's result
    // list, which fires on itemClicked for the same reason).
    connect( list_, &QListView::clicked, this, &SavedSearchesPopup::onIndexActivated );
    connect( list_, &QListView::activated, this, &SavedSearchesPopup::onIndexActivated );

    auto* layout = new QVBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    layout->setSpacing( 0 );
    layout->addWidget( list_ );
}

void SavedSearchesPopup::setItems( const QList<SavedSearch>& items )
{
    model_->clear();
    for ( const auto& item : items ) {
        auto* row = new QStandardItem;
        row->setData( QVariant::fromValue( item ), kRoleSavedSearch );
        model_->appendRow( row );
    }
    if ( items.isEmpty() ) {
        // Empty placeholder row — not selectable so the user gets visual
        // feedback that the list exists but is empty, and learns where
        // to populate it.
        auto* placeholder = new QStandardItem(
            QStringLiteral( "(no saved searches — Tools → Text Searches…)" ) );
        placeholder->setFlags( Qt::NoItemFlags );
        placeholder->setForeground( QApplication::palette().color( QPalette::Disabled,
                                                                   QPalette::Text ) );
        model_->appendRow( placeholder );
    }
}

void SavedSearchesPopup::popupBelow( QWidget* anchor )
{
    if ( !anchor ) {
        return;
    }
    const QPoint pos = anchor->mapToGlobal( QPoint( 0, anchor->height() ) );
    // Cap height so a long list doesn't fill the screen. ~10 rows is
    // plenty for the dropdown use case; the scrollbar handles the rest.
    const int rowHeight = list_->sizeHintForRow( 0 );
    const int visibleRows = std::min( std::max( model_->rowCount(), 1 ), 10 );
    const int frameMargin = 2 * frameWidth();
    const int height = visibleRows * std::max( rowHeight, 18 ) + frameMargin;
    resize( anchor->width(), height );
    move( pos );
    show();
    list_->setFocus();
    if ( model_->rowCount() > 0 ) {
        const QModelIndex first = model_->index( 0, 0 );
        if ( first.flags() & Qt::ItemIsSelectable ) {
            list_->setCurrentIndex( first );
        }
    }
}

void SavedSearchesPopup::keyPressEvent( QKeyEvent* event )
{
    if ( event->key() == Qt::Key_Escape ) {
        close();
        return;
    }
    QFrame::keyPressEvent( event );
}

void SavedSearchesPopup::onIndexActivated( const QModelIndex& index )
{
    if ( !index.isValid() ) {
        return;
    }
    if ( !( index.flags() & Qt::ItemIsSelectable ) ) {
        return;
    }
    const auto search = index.data( kRoleSavedSearch ).value<SavedSearch>();
    close();
    Q_EMIT searchSelected( search );
}
