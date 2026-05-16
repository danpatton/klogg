#include <QCoreApplication>
#include <QFileInfo>
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

    BareTailApp app( argc, argv );

    MainWindow window;
    window.show();

    for ( int i = 1; i < argc; ++i ) {
        const QString path = QString::fromLocal8Bit( argv[ i ] );
        if ( QFileInfo::exists( path ) ) {
            window.openFile( path );
        }
    }

    return app.exec();
}
