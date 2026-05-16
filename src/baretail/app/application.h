#pragma once

#include <QApplication>

// QApplication subclass that runs the engine-init incantation in its ctor
// (the bits NOTES.md flagged in the tail_demo writeup):
//   - qRegisterMetaType for the engine's queued-connection payload types
//   - Configuration::getSynced() before any LogData is constructed
// PersistentInfo::ForcePortable is still defined in main.cpp because it's
// declared extern in klogg_settings.
class BareTailApp : public QApplication {
    Q_OBJECT
  public:
    BareTailApp( int& argc, char** argv );

    // Name of the single HighlighterSet baretail edits (and keeps active) on
    // behalf of the user. Hidden from the UI; we present one flat list of
    // rules per the BareTailPro feel.
    static const char* kRuleSetName;
};
