#include <cstdio>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QString>

#include "persistentinfo.h"

#include "app/application.h"
#include "app/mainwindow.h"

// Declared extern in klogg_settings; every klogg executable defines it.
const bool PersistentInfo::ForcePortable = false;

int main( int argc, char* argv[] )
{
    QCoreApplication::setOrganizationName( "klogg" );
    QCoreApplication::setApplicationName( "baretail" );
    // Used by Wayland (and newer Qt on X11) to set app_id / WM_CLASS so
    // the desktop can match our window to the installed baretail.desktop
    // file and pick up its Icon= entry for the taskbar.
    QGuiApplication::setDesktopFileName( "baretail" );

    BareTailApp app( argc, argv );

    QCommandLineParser parser;
    parser.setApplicationDescription( "BareTail: a log tailer for Linux" );
    parser.addHelpOption();
    parser.addPositionalArgument( "file", "Log file(s) to open.", "[file...]" );

    QCommandLineOption uiFontOption( "ui-font",
        "Set the UI font for buttons, menus, etc. Format: \"Family,size\".",
        "font" );
    parser.addOption( uiFontOption );

    QCommandLineOption listFontsOption( "list-fonts",
        "List the font families Qt knows about, and exit." );
    parser.addOption( listFontsOption );

    parser.process( app );

    if ( parser.isSet( listFontsOption ) ) {
        // QFontDatabase is what Qt itself consults when resolving family
        // names, so this matches what --ui-font will accept.
        for ( const auto& family : QFontDatabase().families() ) {
            std::printf( "%s\n", family.toLocal8Bit().constData() );
        }
        return 0;
    }

    if ( parser.isSet( uiFontOption ) ) {
        QFont uiFont;
        if ( !uiFont.fromString( parser.value( uiFontOption ) ) ) {
            std::fprintf( stderr, "baretail: could not parse --ui-font value '%s'\n",
                          parser.value( uiFontOption ).toLocal8Bit().constData() );
            return 1;
        }
        QApplication::setFont( uiFont );
    }

    // Multi-resolution app icon. Qt picks 16x16 for title bars / tabs,
    // 32x32 for the taskbar at standard DPI; the .ico plugin handles both.
    QIcon appIcon;
    appIcon.addFile( ":/images/baretail/app-16.ico", QSize( 16, 16 ) );
    appIcon.addFile( ":/images/baretail/app-32.ico", QSize( 32, 32 ) );
    QApplication::setWindowIcon( appIcon );

    MainWindow window;
    window.show();

    for ( const auto& path : parser.positionalArguments() ) {
        if ( QFileInfo::exists( path ) ) {
            window.openFile( path );
        }
    }

    return app.exec();
}
