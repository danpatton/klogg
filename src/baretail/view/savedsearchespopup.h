#pragma once

#include <QFrame>
#include <QList>

#include "savedsearches.h"

class QListView;
class QStandardItemModel;
class QWidget;

// Drop-down popup attached to the SearchPane's text input. Shows the list
// of SavedSearches; each row is painted by SavedSearchDelegate as
// "name | pattern | (flags)" matching the BareTailPro Text Searches
// dropdown. Closes on selection, Escape, or losing focus (Qt::Popup
// handles the last one automatically).
class SavedSearchesPopup : public QFrame {
    Q_OBJECT
  public:
    explicit SavedSearchesPopup( QWidget* parent = nullptr );

    // Replace the displayed list with `items`. Cheap; safe to call on
    // every show so the popup always reflects the persisted state.
    void setItems( const QList<SavedSearch>& items );

    // Show below `anchor`, sized to its width. Called by SearchPane on
    // the dropdown button click.
    void popupBelow( QWidget* anchor );

  Q_SIGNALS:
    void searchSelected( const SavedSearch& search );

  protected:
    void keyPressEvent( QKeyEvent* event ) override;

  private Q_SLOTS:
    void onIndexActivated( const QModelIndex& index );

  private:
    QListView* list_;
    QStandardItemModel* model_;
};
