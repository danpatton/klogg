#pragma once

#include <memory>

#include <QWidget>

#include "linetypes.h"
#include "loadingstatus.h"
#include "regularexpressionpattern.h"

class LogData;
class LogFilteredData;
class LogMainView;
class QuickFindPattern;
class SearchPane;

// Per-tab document: owns a klogg LogData engine plus a LogMainView widget
// displaying it. Drives follow-tail and emits line-count updates so the
// status bar can stay in sync. The search pane is always visible below
// the view; its results are populated from a LogFilteredData created on
// demand and reused for the life of the tab.
class TailDocument : public QWidget {
    Q_OBJECT
  public:
    explicit TailDocument( const QString& fileName, QWidget* parent = nullptr );
    ~TailDocument() override;

    QString fileName() const
    {
        return fileName_;
    }
    LinesCount lineCount() const;
    qint64 fileSize() const;
    bool isFollowEnabled() const;

    // MainWindow's Ctrl+F dispatches here on the active tab.
    void focusSearch();

  public Q_SLOTS:
    void setFollowEnabled( bool follow );
    // Force the viewport to repaint. Used after highlighter rules change
    // so updated colours take effect without waiting for the next event.
    void refreshView();

    // Scrolls the viewport, without changing the follow state.
    void jumpToTop();
    void jumpToBottom();

    void applyFont( const QFont& font );

    // Bookmark commands. MainWindow's Bookmarks menu owns the shortcuts and
    // dispatches here on the active tab.
    void toggleBookmark();
    void nextBookmark();
    void prevBookmark();
    void clearBookmarks();

  Q_SIGNALS:
    void linesUpdated( LinesCount total );

  private Q_SLOTS:
    void onLoadingFinished( LoadingStatus status );
    void onMarkLinesRequested( const klogg::vector<LineNumber>& lines );
    void onSearchRequested( const QString& pattern, bool isRegex, bool ignoreCase,
                            bool invertMatch );
    void onStopRequested();
    void onClearRequested();
    void onJumpToLineRequested( LineNumber line );
    void onSearchProgressed( LinesCount nbMatches, int progress, LineNumber initialLine );

  private:
    // Pull any matches we haven't shown yet out of filteredData_ and into
    // the search pane. Called after every searchProgressed signal.
    void appendNewMatches();

    QString fileName_;
    std::unique_ptr<LogData> logData_;
    std::unique_ptr<LogFilteredData> filteredData_;
    std::unique_ptr<QuickFindPattern> qfp_;
    LogMainView* view_;
    SearchPane* searchPane_;

    // Search state. searchActive_ guards filter-tail's updateSearch on
    // appended lines; resultsShown_ is the number of rows already pushed
    // to the pane so we know which new matches to append.
    bool searchActive_ = false;
    RegularExpressionPattern currentPattern_;
    quint64 resultsShown_ = 0;
};
