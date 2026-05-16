#include "mainwindow.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

#include "configuration.h"
#include "taildocument.h"
#include "application.h"
#include "highlightingdialog.h"

MainWindow::MainWindow()
    : tabs_( new QTabWidget( this ) )
    , statusPath_( new QLabel( this ) )
    , statusLines_( new QLabel( this ) )
{
    setWindowTitle( "BareTail" );
    resize( 1100, 700 );

    tabs_->setTabsClosable( true );
    tabs_->setMovable( true );
    tabs_->setDocumentMode( true );
    connect( tabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::onCloseTab );
    connect( tabs_, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged );
    setCentralWidget( tabs_ );

    buildMenus();
    buildToolBar();
    buildStatusBar();
}

void MainWindow::buildMenus()
{
    // Note: under Qt 5 + Wayland, hovering over menu items produces
    // "qt.qpa.wayland: Wayland does not support QWindow::requestActivate()"
    // on stderr. This is QTBUG-77173: QMenu's mouseMoveEvent calls setFocus
    // on the hovered action, whose focus path asks the popup window to
    // self-activate; Wayland's protocol forbids that and QtWayland logs
    // the rejection. Menus still work correctly; we deliberately leave
    // the warning unfiltered.
    auto* fileMenu = menuBar()->addMenu( "&File" );

    auto* openAction = fileMenu->addAction( "&Open..." );
    openAction->setShortcut( QKeySequence::Open );
    connect( openAction, &QAction::triggered, this, &MainWindow::onOpenFile );

    fileMenu->addSeparator();

    auto* quitAction = fileMenu->addAction( "&Quit" );
    quitAction->setShortcut( QKeySequence::Quit );
    connect( quitAction, &QAction::triggered, this, &QWidget::close );

    auto* viewMenu = menuBar()->addMenu( "&View" );

    auto* topAction = viewMenu->addAction( "Top of &File" );
    topAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_Home ) );
    connect( topAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->jumpToTop();
        }
    } );

    auto* bottomAction = viewMenu->addAction( "&Bottom of File" );
    bottomAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_End ) );
    connect( bottomAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->jumpToBottom();
        }
    } );

    auto* searchMenu = menuBar()->addMenu( "&Search" );
    auto* findAction = searchMenu->addAction( "&Find..." );
    findAction->setShortcut( QKeySequence::Find );
    connect( findAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->showFindBar();
        }
    } );

    auto* findNextAction = searchMenu->addAction( "Find &Next" );
    findNextAction->setShortcut( QKeySequence( Qt::Key_F3 ) );
    connect( findNextAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->findNext();
        }
    } );

    auto* findPrevAction = searchMenu->addAction( "Find &Previous" );
    findPrevAction->setShortcut( QKeySequence( Qt::SHIFT | Qt::Key_F3 ) );
    connect( findPrevAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->findPrev();
        }
    } );

    auto* bookmarksMenu = menuBar()->addMenu( "&Bookmarks" );
    auto* toggleBmAction = bookmarksMenu->addAction( "&Toggle Bookmark" );
    toggleBmAction->setShortcut( QKeySequence( Qt::CTRL | Qt::Key_F2 ) );
    connect( toggleBmAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->toggleBookmark();
        }
    } );

    auto* nextBmAction = bookmarksMenu->addAction( "&Next Bookmark" );
    nextBmAction->setShortcut( QKeySequence( Qt::Key_F2 ) );
    connect( nextBmAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->nextBookmark();
        }
    } );

    auto* prevBmAction = bookmarksMenu->addAction( "&Previous Bookmark" );
    prevBmAction->setShortcut( QKeySequence( Qt::SHIFT | Qt::Key_F2 ) );
    connect( prevBmAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->prevBookmark();
        }
    } );

    bookmarksMenu->addSeparator();
    auto* clearBmAction = bookmarksMenu->addAction( "&Clear All Bookmarks" );
    connect( clearBmAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->clearBookmarks();
        }
    } );

    auto* toolsMenu = menuBar()->addMenu( "&Tools" );
    auto* highlightAction = toolsMenu->addAction( "&Highlighting..." );
    connect( highlightAction, &QAction::triggered, this, &MainWindow::onEditHighlighters );

    auto* fontAction = toolsMenu->addAction( "&Font..." );
    connect( fontAction, &QAction::triggered, this, &MainWindow::onChooseFont );
}

void MainWindow::buildToolBar()
{
    auto* bar = addToolBar( "Main" );
    bar->setMovable( false );

    auto* open = bar->addAction( "Open" );
    connect( open, &QAction::triggered, this, &MainWindow::onOpenFile );

    bar->addSeparator();

    followTailAction_ = bar->addAction( "Follow Tail" );
    followTailAction_->setCheckable( true );
    followTailAction_->setChecked( true );
    followTailAction_->setToolTip(
        "Jump to end of file when new lines are appended" );
    connect( followTailAction_, &QAction::toggled, this, &MainWindow::onFollowTailToggled );
}

void MainWindow::buildStatusBar()
{
    statusBar()->addWidget( statusPath_, 1 );
    statusBar()->addPermanentWidget( statusLines_ );
}

void MainWindow::onOpenFile()
{
    const QString fileName
        = QFileDialog::getOpenFileName( this, "Open log file", QString(), "All files (*)" );
    if ( !fileName.isEmpty() ) {
        openFile( fileName );
    }
}

void MainWindow::openFile( const QString& fileName )
{
    auto* doc = new TailDocument( fileName, tabs_ );
    if ( followTailAction_ ) {
        doc->setFollowEnabled( followTailAction_->isChecked() );
    }
    connect( doc, &TailDocument::linesUpdated, this, &MainWindow::onTabLinesUpdated );
    const int idx = tabs_->addTab( doc, QFileInfo( fileName ).fileName() );
    tabs_->setTabToolTip( idx, fileName );
    tabs_->setCurrentIndex( idx );
    refreshStatusBar();
    refreshFollowTailAction();
}

void MainWindow::onCloseTab( int index )
{
    auto* w = tabs_->widget( index );
    tabs_->removeTab( index );
    w->deleteLater();
    refreshStatusBar();
}

void MainWindow::onCurrentTabChanged( int /*index*/ )
{
    refreshStatusBar();
    refreshFollowTailAction();
}

void MainWindow::onFollowTailToggled( bool checked )
{
    if ( auto* doc = currentDocument() ) {
        doc->setFollowEnabled( checked );
    }
}

void MainWindow::refreshFollowTailAction()
{
    if ( !followTailAction_ ) {
        return;
    }
    QSignalBlocker blocker( followTailAction_ );
    if ( auto* doc = currentDocument() ) {
        followTailAction_->setEnabled( true );
        followTailAction_->setChecked( doc->isFollowEnabled() );
    }
    else {
        followTailAction_->setEnabled( false );
    }
}

void MainWindow::onTabLinesUpdated()
{
    // The status bar only ever reflects the visible tab. If another tab
    // emits an update, ignore it.
    if ( sender() == currentDocument() ) {
        refreshStatusBar();
    }
}

void MainWindow::onChooseFont()
{
    auto& config = Configuration::get();
    bool ok = false;
    const QFont chosen = QFontDialog::getFont( &ok, config.mainFont(), this,
                                               "Choose viewport font" );
    if ( !ok ) {
        return;
    }
    config.setMainFont( chosen );
    config.save();
    for ( int i = 0; i < tabs_->count(); ++i ) {
        if ( auto* doc = qobject_cast<TailDocument*>( tabs_->widget( i ) ) ) {
            doc->applyFont( chosen );
        }
    }
}

void MainWindow::onEditHighlighters()
{
    HighlightingDialog dialog( BareTailApp::kRuleSetName, this );
    connect( &dialog, &HighlightingDialog::rulesChanged, this, [ this ]() {
        for ( int i = 0; i < tabs_->count(); ++i ) {
            if ( auto* doc = qobject_cast<TailDocument*>( tabs_->widget( i ) ) ) {
                doc->refreshView();
            }
        }
    } );
    dialog.exec();
}

TailDocument* MainWindow::currentDocument() const
{
    return qobject_cast<TailDocument*>( tabs_->currentWidget() );
}

void MainWindow::refreshStatusBar()
{
    if ( auto* doc = currentDocument() ) {
        statusPath_->setText( doc->fileName() );
        statusLines_->setText( QString( "%1 lines" ).arg( doc->lineCount().get() ) );
    }
    else {
        statusPath_->clear();
        statusLines_->clear();
    }
}
