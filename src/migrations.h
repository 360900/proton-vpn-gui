#pragma once

#include <QString>

// Migrations – one-time tasks that run at startup after an upgrade.
//
// Adding a migration:
//   1. Declare the function here.
//   2. Implement it in migrations.cpp and add a call in run().
//
// Removing a migration:
//   1. Delete the implementation in migrations.cpp.
//   2. Delete the call from run().
//   3. Delete the declaration below.

namespace Migrations
{
    // Master function
    void run(const QString& previousVersion);

    // Removes the legacy systemd user service file and, if auto-start was
    // active, creates an equivalent XDG autostart .desktop entry.
    void migrateSystemdToXdgAutostart(const QString& previousVersion);
}
