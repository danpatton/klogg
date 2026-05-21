// Throwaway proof-of-concept: drive klogg's LogData engine standalone and
// print appended lines to stdout. See NOTES.md.

#include <iostream>

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>

#include "configuration.h"
#include "loadingstatus.h"
#include "logdata.h"
#include "persistentinfo.h"

// PersistentInfo declares this extern; every klogg executable defines it.
const bool PersistentInfo::ForcePortable = false;

int main( int argc, char* argv[] )
{
    QCoreApplication::setOrganizationName( "klogg" );
    QCoreApplication::setApplicationName( "tail_demo" );

    QCoreApplication app( argc, argv );

    qRegisterMetaType<LoadingStatus>( "LoadingStatus" );
    qRegisterMetaType<MonitoredFileStatus>( "MonitoredFileStatus" );
    qRegisterMetaType<LinesCount>( "LinesCount" );
    qRegisterMetaType<LineNumber>( "LineNumber" );

    // LogData's ctor reads Configuration::get(); the singleton must be
    // initialised via getSynced() first or it throws.
    Configuration::getSynced();

    if ( argc < 2 ) {
        std::cerr << "usage: tail_demo <file>\n";
        return 1;
    }

    const QString fileName = QString::fromLocal8Bit( argv[ 1 ] );
    if ( !QFileInfo::exists( fileName ) ) {
        std::cerr << "no such file: " << argv[ 1 ] << '\n';
        return 1;
    }

    LogData logData;
    LineNumber lastPrinted{ 0 };

    auto flushNewLines = [ & ] {
        const LinesCount total = logData.getNbLine();
        const LinesCount alreadyPrinted{ lastPrinted.get() };
        if ( total <= alreadyPrinted ) {
            return;
        }
        const LinesCount toPrint = total - alreadyPrinted;
        const auto lines = logData.getLines( lastPrinted, toPrint );
        for ( const auto& line : lines ) {
            std::cout << line.toStdString() << '\n';
        }
        std::cout.flush();
        lastPrinted += toPrint;
    };

    // loadingFinished fires after the initial index AND after every partial
    // reindex triggered by the FileWatcher seeing the file change on disk.
    QObject::connect( &logData, &LogData::loadingFinished, &app,
                      [ & ]( LoadingStatus status ) {
                          if ( status == LoadingStatus::Successful ) {
                              flushNewLines();
                          }
                      } );

    logData.attachFile( fileName );
    return app.exec();
}
