#pragma once
// appImageUtils.h
// Utilities for detecting and adapting to an AppImage runtime environment.

#include <QString>

// Returns true when this process is running inside an AppImage.
// The APPIMAGE environment variable is set by the AppImage runtime.
inline bool isRunningAsAppImage()
{
    return qEnvironmentVariableIsSet("APPIMAGE");
}

