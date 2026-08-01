#pragma once
// appImageUtils.h
// Utilities for detecting and adapting to an AppImage runtime environment.

#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

// Returns true when this process is running inside an AppImage.
// The APPIMAGE environment variable is set by the AppImage runtime.
inline bool isRunningAsAppImage()
{
    return qEnvironmentVariableIsSet("APPIMAGE");
}
// Returns true when this is the Standalone AppImage variant, identified by
// the presence of the bundled CLI launcher AppRun places on PATH. The Lite
// AppImage never ships this launcher, since it relies on a host CLI install.
inline bool isStandaloneAppImage()
{
    if (!isRunningAsAppImage())
    {
        return false;
    }
    const QString bundledLauncher =
        qEnvironmentVariable("APPDIR") + QStringLiteral("/usr/share/protonvpn/protonvpn");
    return QFileInfo::exists(bundledLauncher);
}

// Returns true when a "protonvpn" executable is installed somewhere on PATH
// other than the Standalone AppImage's own bundled CLI directory (which
// AppRun prepends to PATH, so a plain PATH lookup would always find the
// bundled copy first regardless of whether a separate system install exists).
inline bool systemProtonVpnCliInstalledSeparately()
{
    const QString bundledDir =
        qEnvironmentVariable("APPDIR") + QStringLiteral("/usr/share/protonvpn");
    QStringList searchDirs = qEnvironmentVariable("PATH").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    searchDirs.removeAll(bundledDir);
    return !QStandardPaths::findExecutable(QStringLiteral("protonvpn"), searchDirs).isEmpty();
}
