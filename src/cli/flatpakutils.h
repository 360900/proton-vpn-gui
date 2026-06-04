#pragma once
// flatpakutils.h
// Shared utilities for detecting and adapting to a Flatpak sandbox environment.

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <utility>

// Returns true when this process is running inside a Flatpak sandbox.
// The FLATPAK_ID environment variable is always set by the Flatpak runtime.
inline bool isRunningAsFlatpak()
{
    return qEnvironmentVariableIsSet("FLATPAK_ID");
}

// Returns {program, fullArgs} to run an arbitrary host command via QProcess.
// When running inside a Flatpak sandbox, the command is forwarded to the host
//  via flatpak-spawn --host. Outside of Flatpak, it is returned unchanged.

// Example:
//   auto [prog, args] = buildHostCommand("systemctl", {"--user", "enable", "foo"});
//   process.start(prog, args);
inline std::pair<QString, QStringList> buildHostCommand(const QString& program,
                                                        const QStringList& args = {})
{
    if (isRunningAsFlatpak())
    {
        QStringList spawnArgs;
        spawnArgs << QStringLiteral("--host") << program << args;
        return {QStringLiteral("flatpak-spawn"), spawnArgs};
    }
    return {program, args};
}
