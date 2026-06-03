#pragma once

// debug.h - Lightweight diagnostic logging helpers.
// Prints to stdout when the app is started from a terminal.
// All output is prefixed with a tag for easy grepping.

#include <QString>
#include <QDateTime>
#include <cstdio>

//  Core print function

inline void dbgPrint(const char* tag, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    fprintf(stdout, "[%s] [%s] %s\n",
            qPrintable(timestamp),
            tag,
            qPrintable(message));
    fflush(stdout);
}

//  Convenience macros

#define DBG_APP(msg)      dbgPrint("APP     ", (msg))
#define DBG_CLI(msg)      dbgPrint("CLI     ", (msg))
#define DBG_SETTINGS(msg) dbgPrint("SETTINGS", (msg))
#define DBG_STATUS(msg)   dbgPrint("STATUS  ", (msg))
#define DBG_POLL(msg)     dbgPrint("POLL    ", (msg))

