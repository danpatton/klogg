#pragma once

#include <memory>

#include <QWidget>

#include "linetypes.h"
#include "loadingstatus.h"

class LogData;
class LogFilteredData;
class LogMainView;
class QLineEdit;
class QToolButton;
class QuickFindPattern;
class QWidget;

#include "linetypes.h"

// Per-tab document: owns a klogg LogData engine plus a LogMainView widget
// displaying it. Drives follow-tail and emits line-count updates so the
// status bar can stay in sync.
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
    bool isFollowEnabled() const;

  public Q_SLOTS:
    void setFollowEnabled( bool follow );
    // Force the viewport to repaint. Used after highlighter rules change
    // so updated colours take effect without waiting for the next event.
    void refreshView();

    // Scrolls the viewport, without changing the follow state.
    void jumpToTop();
    void jumpToBottom();

    void applyFont( const QFont& font );

    // Find-bar commands. MainWindow's Search menu owns the shortcuts and
    // dispatches here on the active tab.
    void showFindBar();
    void findNext();
    void findPrev();

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
    void onFindTextChanged( const QString& text );
    void hideFindBar();
    void onMarkLinesRequested( const klogg::vector<LineNumber>& lines );

  private:
    void buildFindBar();

    QString fileName_;
    std::unique_ptr<LogData> logData_;
    std::unique_ptr<LogFilteredData> filteredData_;
    std::unique_ptr<QuickFindPattern> qfp_;
    LogMainView* view_;

    QWidget* findBar_ = nullptr;
    QLineEdit* findInput_ = nullptr;
    QToolButton* findNextBtn_ = nullptr;
    QToolButton* findPrevBtn_ = nullptr;
    QToolButton* findCloseBtn_ = nullptr;
};
