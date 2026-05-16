#pragma once

#include <QMainWindow>

class QAction;
class QTabWidget;
class QLabel;
class TailDocument;

class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    MainWindow();

    // Opens fileName in a new tab. Safe to call with a non-existent path;
    // the engine reports an empty file in that case.
    void openFile( const QString& fileName );

  private Q_SLOTS:
    void onOpenFile();
    void onCloseTab( int index );
    void onCurrentTabChanged( int index );
    void onTabLinesUpdated();
    void onEditHighlighters();
    void onFollowTailToggled( bool checked );
    void onChooseFont();

  private:
    TailDocument* currentDocument() const;
    void refreshStatusBar();
    void refreshFollowTailAction();

  private:
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();

    QTabWidget* tabs_;
    QLabel* statusPath_;
    QLabel* statusLines_;
    QAction* followTailAction_ = nullptr;
};
