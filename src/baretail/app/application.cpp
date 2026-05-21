#include "application.h"

#include "configuration.h"
#include "highlighterset.h"
#include "linetypes.h"
#include "loadingstatus.h"
#include "savedsearches.h"
#include "selection.h"

const char* BareTailApp::kRuleSetName = "BareTail";

namespace {

// Make sure HighlighterSetCollection contains a set with the BareTail name
// and that it's the currently-active set. baretail edits this single set and
// keeps the multi-set machinery hidden from the user.
void ensureBareTailRuleSet()
{
    auto& collection = HighlighterSetCollection::get();
    auto sets = collection.highlighterSets();

    QString id;
    for ( const auto& set : sets ) {
        if ( set.name() == BareTailApp::kRuleSetName ) {
            id = set.id();
            break;
        }
    }

    if ( id.isEmpty() ) {
        auto fresh = HighlighterSet::createNewSet( BareTailApp::kRuleSetName );
        id = fresh.id();
        sets.append( fresh );
        collection.setHighlighterSets( sets );
    }

    collection.deactivateAll();
    collection.activateSet( id );
    collection.save();
}

} // namespace

BareTailApp::BareTailApp( int& argc, char** argv )
    : QApplication( argc, argv )
{
    qRegisterMetaType<LoadingStatus>( "LoadingStatus" );
    qRegisterMetaType<MonitoredFileStatus>( "MonitoredFileStatus" );
    qRegisterMetaType<LinesCount>( "LinesCount" );
    qRegisterMetaType<LineNumber>( "LineNumber" );
    qRegisterMetaType<LineLength>( "LineLength" );
    // QuickFind::searchDone(bool, Portion) is a queued connection; Qt needs
    // Portion in the metatype registry to marshal it across the queue.
    qRegisterMetaType<Portion>( "Portion" );

    // Persistable singletons must be loaded before any consumer accesses them.
    // AbstractLogView's paint path reads from both: Configuration for colours
    // and wrap settings, HighlighterSetCollection for active highlight rules.
    Configuration::getSynced();
    HighlighterSetCollection::getSynced();
    SavedSearches::getSynced();

    ensureBareTailRuleSet();
}
