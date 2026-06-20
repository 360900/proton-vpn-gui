#pragma once
// platformUtils.h
// Utilities for identifying the current packaging/runtime environment.

#include "appImageUtils.h"
#include "flatpakUtils.h"
#include <QString>

// Returns a human-readable string identifying the current packaging format:
// "Flatpak", "AppImage", or "System".
inline QString packageTypeName()
{
    if (isRunningAsFlatpak())
    {
        return QStringLiteral("Flatpak");
    }
    if (isRunningAsAppImage())
    {
        return QStringLiteral("AppImage");
    }
    return QStringLiteral("System");
}
