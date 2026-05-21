#pragma once

#include <QDialog>
#include <QList>

#include "savedsearches.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

// Editor for the SavedSearches persistable: flat list of named searches,
// each with a pattern and three behaviour flags (regex, ignore-case,
// invert-match). Structurally a clone of HighlightingDialog, which is
// itself the BareTailPro-style flat-list editor pattern.
class TextSearchesDialog : public QDialog {
    Q_OBJECT
  public:
    explicit TextSearchesDialog( QWidget* parent = nullptr );

  Q_SIGNALS:
    // Fired on OK after the list is persisted. The SearchPane's dropdown
    // reads from SavedSearches::get() on next open, so consumers don't
    // strictly need this; it's exposed for parity with HighlightingDialog
    // in case a future caller wants to react.
    void searchesChanged();

  private Q_SLOTS:
    void onAdd();
    void onDelete();
    void onMoveUp();
    void onMoveDown();
    void onCurrentRowChanged( int row );
    void onNameEdited( const QString& text );
    void onPatternEdited( const QString& text );
    void onRegexToggled( bool checked );
    void onIgnoreCaseToggled( bool checked );
    void onInvertMatchToggled( bool checked );
    void onAccept();

  private:
    void buildUi();
    void loadFromStorage();
    void refreshRow( int row );
    void refreshFields();
    void refreshButtons();
    int currentRow() const;

    QList<SavedSearch> searches_;

    QTableWidget* table_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* moveUpBtn_ = nullptr;
    QPushButton* moveDownBtn_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* patternEdit_ = nullptr;
    QCheckBox* regexCheck_ = nullptr;
    QCheckBox* ignoreCaseCheck_ = nullptr;
    QCheckBox* invertMatchCheck_ = nullptr;
};
