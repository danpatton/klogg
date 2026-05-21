#pragma once

#include <QMainWindow>

class QAction;
class QCheckBox;
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
    void onEditTextSearches();
    void onFollowTailToggled( bool checked );
    void onChooseFont();
    void onShowStatusBarToggled( bool checked );

  private:
    TailDocument* currentDocument() const;
    void refreshStatusBar();
    void refreshToolBarPath();
    void refreshFollowTailCheck();

  private:
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();

    QTabWidget* tabs_;
    QLabel* statusLines_;
    QLabel* toolbarPath_;
    QCheckBox* followTailCheck_ = nullptr;
    QAction* showStatusBarAction_ = nullptr;
};
