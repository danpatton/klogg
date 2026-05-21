#include "mainwindow.h"

#include <QAction>
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDialog>
#include <QLabel>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QWidget>

#include "configuration.h"
#include "iconloader.h"
#include "readablesize.h"
#include "taildocument.h"
#include "application.h"
#include "highlightingdialog.h"
#include "textsearchesdialog.h"

namespace {
// QSettings key under the org/app namespace set in main.cpp. Persisting
// only the few view-state bits we actually care about — no need to drag
// in klogg's Configuration machinery for one boolean.
constexpr auto kShowStatusBarKey = "view/showStatusBar";
}

MainWindow::MainWindow()
    : tabs_( new QTabWidget( this ) )
    , statusLines_( new QLabel( this ) )
    , toolbarPath_( new QLabel( this ) )
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

    // Restore the persisted status-bar visibility after both the action
    // and the status bar widget exist; the action's setChecked drives the
    // bar through onShowStatusBarToggled.
    QSettings settings;
    const bool showStatusBar = settings.value( kShowStatusBarKey, true ).toBool();
    showStatusBarAction_->setChecked( showStatusBar );
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

    viewMenu->addSeparator();

    showStatusBarAction_ = viewMenu->addAction( "Show &Status Bar" );
    showStatusBarAction_->setCheckable( true );
    // Default true; the actual persisted value is applied in the ctor
    // once the status bar widget itself exists.
    showStatusBarAction_->setChecked( true );
    connect( showStatusBarAction_, &QAction::toggled,
             this, &MainWindow::onShowStatusBarToggled );

    auto* findAction = new QAction( this );
    findAction->setShortcut( QKeySequence::Find );
    findAction->setShortcutContext( Qt::WindowShortcut );
    connect( findAction, &QAction::triggered, this, [ this ]() {
        if ( auto* doc = currentDocument() ) {
            doc->focusSearch();
        }
    } );
    addAction( findAction );

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

    auto* prefsMenu = menuBar()->addMenu( "&Preferences" );
    auto* highlightAction = prefsMenu->addAction( "&Highlighting..." );
    connect( highlightAction, &QAction::triggered, this, &MainWindow::onEditHighlighters );

    auto* searchesAction = prefsMenu->addAction( "Text &Searches..." );
    connect( searchesAction, &QAction::triggered, this, &MainWindow::onEditTextSearches );

    auto* fontAction = prefsMenu->addAction( "&Font..." );
    connect( fontAction, &QAction::triggered, this, &MainWindow::onChooseFont );
}

void MainWindow::buildToolBar()
{
    auto* bar = addToolBar( "Main" );
    bar->setMovable( false );
    // Pin icon size to the actual PNG dimensions so the active style doesn't
    // upscale them. Toolbar spacing / button padding are left to the style.
    bar->setIconSize( QSize( 16, 16 ) );
    // Show "Open" / "Highlighting" next to their icons. Only affects
    // QAction-backed buttons; the Follow Tail QCheckBox added below is
    // unaffected.
    bar->setToolButtonStyle( Qt::ToolButtonTextBesideIcon );

    IconLoader iconLoader( this );

    auto* open = bar->addAction( iconLoader.load( "baretail/open" ), "Open" );
    // QAction defaults its tooltip to the action text; explicit empty
    // string suppresses the redundant "Open" hover.
    open->setToolTip( QStringLiteral( "" ) );
    connect( open, &QAction::triggered, this, &MainWindow::onOpenFile );

    auto* highlight = bar->addAction( iconLoader.load( "baretail/highlight" ), "Highlighting" );
    highlight->setToolTip( QStringLiteral( "" ) );
    connect( highlight, &QAction::triggered, this, &MainWindow::onEditHighlighters );

    // bar->addSeparator();

    followTailCheck_ = new QCheckBox( "Follow Tail", this );
    followTailCheck_->setChecked( true );
    followTailCheck_->setToolTip(
        "Jump to end of file when new lines are appended" );
    connect( followTailCheck_, &QCheckBox::toggled, this, &MainWindow::onFollowTailToggled );
    bar->addWidget( followTailCheck_ );

    // bar->addSeparator();

    toolbarPath_->setTextInteractionFlags( Qt::TextSelectableByMouse );
    toolbarPath_->setMinimumWidth( 0 );
    // sizePolicy ensures the label gets the leftover stretch room; without
    // expanding policy it sits flush against the previous separator.
    toolbarPath_->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
    bar->addWidget( toolbarPath_ );
}

void MainWindow::buildStatusBar()
{
    statusBar()->addPermanentWidget( statusLines_ );
}

void MainWindow::onShowStatusBarToggled( bool checked )
{
    statusBar()->setVisible( checked );
    QSettings settings;
    settings.setValue( kShowStatusBarKey, checked );
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
    if ( followTailCheck_ ) {
        doc->setFollowEnabled( followTailCheck_->isChecked() );
    }
    connect( doc, &TailDocument::linesUpdated, this, &MainWindow::onTabLinesUpdated );
    const int idx = tabs_->addTab( doc, QFileInfo( fileName ).fileName() );
    tabs_->setTabToolTip( idx, fileName );
    tabs_->setCurrentIndex( idx );
    refreshStatusBar();
    refreshToolBarPath();
    refreshFollowTailCheck();
}

void MainWindow::onCloseTab( int index )
{
    auto* w = tabs_->widget( index );
    tabs_->removeTab( index );
    w->deleteLater();
    refreshStatusBar();
    refreshToolBarPath();
}

void MainWindow::onCurrentTabChanged( int /*index*/ )
{
    refreshStatusBar();
    refreshToolBarPath();
    refreshFollowTailCheck();
}

void MainWindow::onFollowTailToggled( bool checked )
{
    if ( auto* doc = currentDocument() ) {
        doc->setFollowEnabled( checked );
    }
}

void MainWindow::refreshFollowTailCheck()
{
    if ( !followTailCheck_ ) {
        return;
    }
    QSignalBlocker blocker( followTailCheck_ );
    if ( auto* doc = currentDocument() ) {
        followTailCheck_->setEnabled( true );
        followTailCheck_->setChecked( doc->isFollowEnabled() );
    }
    else {
        followTailCheck_->setEnabled( false );
    }
}

void MainWindow::onTabLinesUpdated()
{
    // The status bar / toolbar path only ever reflect the visible tab.
    // If another tab emits an update, ignore it.
    if ( sender() == currentDocument() ) {
        refreshStatusBar();
        refreshToolBarPath();
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

void MainWindow::onEditTextSearches()
{
    TextSearchesDialog dialog( this );
    dialog.exec();
}

TailDocument* MainWindow::currentDocument() const
{
    return qobject_cast<TailDocument*>( tabs_->currentWidget() );
}

void MainWindow::refreshStatusBar()
{
    if ( auto* doc = currentDocument() ) {
        statusLines_->setText( QString( "%1 lines" ).arg( doc->lineCount().get() ) );
    }
    else {
        statusLines_->clear();
    }
}

void MainWindow::refreshToolBarPath()
{
    if ( auto* doc = currentDocument() ) {
        const auto size = doc->fileSize();
        const QString sizeText
            = size >= 0 ? readableSize( static_cast<uint64_t>( size ) ) : QString( "?" );
        toolbarPath_->setText( QString( "  %1  (%2)" ).arg( doc->fileName(), sizeText ) );
    }
    else {
        toolbarPath_->clear();
    }
}
