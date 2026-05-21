#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

#include "persistable.h"

class QSettings;

// One persisted search entry. Mirrors the four user-controllable bits of a
// SearchPane request: pattern + the three behaviour flags. `name` is purely
// cosmetic — it labels the entry in the saved-searches dropdown and dialog.
struct SavedSearch {
    QString name;
    QString pattern;
    bool isRegex = false;
    bool ignoreCase = false;
    bool invertMatch = false;
};

Q_DECLARE_METATYPE( SavedSearch )

// Ordered list of saved searches, persisted via QSettings under the same
// org/app namespace used by HighlighterSetCollection. Singleton accessed via
// Persistable::get() / getSynced(); BareTailApp bootstraps it at startup.
class SavedSearches final : public Persistable<SavedSearches> {
  public:
    static const char* persistableName()
    {
        return "SavedSearches";
    }

    const QList<SavedSearch>& items() const;
    void setItems( QList<SavedSearch> items );

    void saveToStorage( QSettings& settings ) const;
    void retrieveFromStorage( QSettings& settings );

  private:
    static constexpr int SavedSearches_VERSION = 1;

    QList<SavedSearch> items_;
};
