#pragma once

#include <QWidget>

#include "linetypes.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QTimer;
class QToolButton;
class QTreeWidget;

// Always-visible search pane docked below the tail. Owns the search input,
// result list, and the option toggles; emits high-level requests that the
// TailDocument routes into LogFilteredData. The pane has no notion of the
// engine, of files, or of the main view.
//
// The debounce timer is what lets typing into the search box re-trigger a
// search without spawning a new worker on every keystroke. Pressing Enter
// or the Search button bypasses the timer; Clear stops + empties results.
class SearchPane : public QWidget {
    Q_OBJECT
  public:
    explicit SearchPane( QWidget* parent = nullptr );

    // Focus the search input (used by Ctrl+F).
    void focusSearchInput();

    // Apply a font to the result list only — matches the viewport's font
    // so results render at the same metrics as the lines they came from.
    void applyResultsFont( const QFont& font );

    bool isFilterTailEnabled() const;

  Q_SIGNALS:
    // Fired when the user explicitly asks for a search, or after the
    // type-to-search debounce fires with non-empty text.
    void searchRequested( const QString& pattern, bool isRegex, bool ignoreCase,
                          bool invertMatch );
    // Fired by the Stop button.
    void stopRequested();
    // Fired by Clear (also empties the result list locally).
    void clearRequested();
    // Fired when the Filter Tail checkbox is toggled; the document watches
    // this so it can drive updateSearch() on appended lines.
    void filterTailToggled( bool enabled );
    // Fired when the user activates a row in the result list.
    void jumpToLineRequested( LineNumber line );

  public Q_SLOTS:
    // Append one result row. Called by the document as matches stream in.
    void appendResult( LineNumber line, const QString& text );
    // Wipe the result list (and the status label).
    void clearResults();
    // Status text shown on the toolbar row, e.g. "Found 300 matching
    // lines so far...". Empty string hides it.
    void setStatusText( const QString& text );

  private Q_SLOTS:
    void onSearchTextChanged( const QString& text );
    void onDebounceFired();
    void onSearchClicked();
    void onStopClicked();
    void onClearClicked();
    void onResultActivated();

  private:
    void emitSearchRequest();

    QLineEdit* input_;
    QToolButton* searchBtn_;
    QToolButton* stopBtn_;
    QToolButton* clearBtn_;
    QCheckBox* regexCheck_;
    QCheckBox* ignoreCaseCheck_;
    QCheckBox* invertMatchCheck_;
    QCheckBox* filterTailCheck_;
    QLabel* statusLabel_;
    QTreeWidget* resultList_;
    QTimer* debounce_;
};
