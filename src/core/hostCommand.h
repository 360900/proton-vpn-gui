#pragma once
// hostCommand.h
// Shared utilities for detecting a Flatpak sandbox environment.
//
// The protonvpn CLI is bundled inside the sandbox, so every QProcess spawn goes
// through buildHostCommand() (or a ProcessRunner, which calls it) which always
// runs the bundled program directly in-sandbox. No flatpak-spawn --host
// delegation is needed anymore.

#include "appImageUtils.h"

#include <QString>
#include <QStringList>
#include <utility>

// Returns true when this process is running inside a Flatpak sandbox.
// The FLATPAK_ID environment variable is always set by the Flatpak runtime.
inline bool isRunningAsFlatpak()
{
    return qEnvironmentVariableIsSet("FLATPAK_ID");
}

// Returns {program, fullArgs} to run a command via QProcess.
// The bundled CLI is on PATH both natively and inside the sandbox, so the
// program is always returned unchanged (previously it was forwarded to the
// host via flatpak-spawn --host; that delegation no longer exists).
//
// Example:
//   auto [prog, args] = buildHostCommand("protonvpn", {"connect", country});
//   process.start(prog, args);
inline std::pair<QString, QStringList> buildHostCommand(const QString& program,
                                                        const QStringList& args = {})
{
    return {program, args};
}
