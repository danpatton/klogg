#pragma once

#include <QDialog>
#include <QList>

#include "highlighterset.h"

class QCheckBox;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QToolButton;

// BareTailPro-style flat-list highlight editor. Edits a single named
// HighlighterSet inside HighlighterSetCollection (hides klogg's multi-set
// concept from the UI per the project's BareTailPro-parity goal).
//
// Emits rulesChanged() on OK so open tabs can repaint.
class HighlightingDialog : public QDialog {
    Q_OBJECT
  public:
    // setName is the name of the HighlighterSet to edit. The set must already
    // exist in HighlighterSetCollection (BareTailApp bootstraps it at startup).
    explicit HighlightingDialog( const QString& setName, QWidget* parent = nullptr );

  Q_SIGNALS:
    void rulesChanged();

  private Q_SLOTS:
    void onAdd();
    void onDelete();
    void onMoveUp();
    void onMoveDown();
    void onCurrentRowChanged( int row );
    void onPatternEdited( const QString& text );
    void onIgnoreCaseToggled( bool checked );
    void onPickForeColor();
    void onPickBackColor();
    void onAccept();

  private:
    void buildUi();
    void loadFromCollection();
    void refreshRow( int row );
    void refreshFields();
    void refreshButtons();
    void refreshColorButton( QToolButton* button, const QColor& color );
    int currentRow() const;

    QString setName_;
    QList<Highlighter> rules_;

    QTableWidget* table_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* deleteBtn_ = nullptr;
    QPushButton* moveUpBtn_ = nullptr;
    QPushButton* moveDownBtn_ = nullptr;
    QToolButton* foreColorBtn_ = nullptr;
    QToolButton* backColorBtn_ = nullptr;
    QLineEdit* patternEdit_ = nullptr;
    QCheckBox* ignoreCaseCheck_ = nullptr;
};
