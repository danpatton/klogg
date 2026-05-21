#include "savedsearches.h"

#include <QSettings>

#include "log.h"

const QList<SavedSearch>& SavedSearches::items() const
{
    return items_;
}

void SavedSearches::setItems( QList<SavedSearch> items )
{
    items_ = std::move( items );
}

void SavedSearches::saveToStorage( QSettings& settings ) const
{
    LOG_INFO << "SavedSearches::saveToStorage, v" << SavedSearches_VERSION;

    settings.beginGroup( "SavedSearches" );
    settings.setValue( "version", SavedSearches_VERSION );
    settings.remove( "items" );
    settings.beginWriteArray( "items" );
    for ( int i = 0; i < items_.size(); ++i ) {
        settings.setArrayIndex( i );
        settings.setValue( "name", items_[ i ].name );
        settings.setValue( "pattern", items_[ i ].pattern );
        settings.setValue( "regex", items_[ i ].isRegex );
        settings.setValue( "ignore_case", items_[ i ].ignoreCase );
        settings.setValue( "invert_match", items_[ i ].invertMatch );
    }
    settings.endArray();
    settings.endGroup();
}

void SavedSearches::retrieveFromStorage( QSettings& settings )
{
    LOG_DEBUG << "SavedSearches::retrieveFromStorage";

    items_.clear();

    if ( !settings.contains( "SavedSearches/version" ) ) {
        return;
    }

    settings.beginGroup( "SavedSearches" );
    if ( settings.value( "version" ).toInt() <= SavedSearches_VERSION ) {
        const int size = settings.beginReadArray( "items" );
        for ( int i = 0; i < size; ++i ) {
            settings.setArrayIndex( i );
            SavedSearch item;
            item.name = settings.value( "name" ).toString();
            item.pattern = settings.value( "pattern" ).toString();
            item.isRegex = settings.value( "regex", false ).toBool();
            item.ignoreCase = settings.value( "ignore_case", false ).toBool();
            item.invertMatch = settings.value( "invert_match", false ).toBool();
            items_.append( std::move( item ) );
        }
        settings.endArray();
    }
    else {
        LOG_ERROR << "Unknown version of SavedSearches, ignoring it...";
    }
    settings.endGroup();
}
