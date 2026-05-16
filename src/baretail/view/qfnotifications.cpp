// Static QString definitions for QFNotification. In klogg these live in
// quickfindwidget.cpp; we don't carry that widget so they need a home of
// their own.
#include "qfnotifications.h"

const QString QFNotification::REACHED_EOF = "Reached end of file, no occurrence found.";
const QString QFNotification::REACHED_BOF = "Reached beginning of file, no occurrence found.";
const QString QFNotification::INTERRUPTED = "Search interrupted";
