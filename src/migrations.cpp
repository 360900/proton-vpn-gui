#include "migrations.h"
#include "cli/flatpakUtils.h"
#include "debug.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVersionNumber>

namespace
{
#ifdef QT_DEBUG
constexpr bool DRY_RUN_MODE = true;
#else
constexpr bool DRY_RUN_MODE = false;
#endif

// The last version that used a systemd user service for auto-start.
// Versions after this switched to an XDG autostart .desktop file.
const QVersionNumber LAST_SYSTEMD_VERSION(1, 9, 0);
} // namespace

namespace Migrations
{

void run(const QString& previousVersion)
{
    migrateSystemdToXdgAutostart(previousVersion);
}

void migrateSystemdToXdgAutostart(const QString& previousVersion)
{
    const QVersionNumber prev = QVersionNumber::fromString(previousVersion);
    // Skip on a fresh install (empty previousVersion) or if already past the
    // migration point.
    if (prev.isNull() || prev > LAST_SYSTEMD_VERSION) return;

    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString serviceFile = configDir + QStringLiteral("/systemd/user/proton-vpn-gui.service");

    // If the service file does not exist, auto-start was not enabled under the
    // old version; remove nothing, create nothing.
    if (QFileInfo::exists(serviceFile) == false) return;

    DBG_APP(QStringLiteral("Migration: replacing legacy systemd unit with XDG autostart entry"));

    if constexpr (DRY_RUN_MODE == true)
    {
        DBG_APP(QStringLiteral("[DRY RUN] Would remove: ") + serviceFile);
        DBG_APP(QStringLiteral("[DRY RUN] Would write XDG autostart entry"));
        return;
    }

    QFile::remove(serviceFile);

    // Load the bundled .desktop template.
    QFile templateFile(QStringLiteral(":/autostart/proton-vpn-gui.desktop"));
    if (templateFile.open(QIODevice::ReadOnly) == false)
    {
        DBG_APP(QStringLiteral("Migration: failed to read autostart template resource"));
        return;
    }
    QString content = QString::fromUtf8(templateFile.readAll());
    templateFile.close();

    // Substitute the executable path placeholder. The AppImage runtime mounts
    // under /tmp/.mount_XXXX (gone after unmount); $APPIMAGE survives reboot.
    const QString exec = isRunningAsFlatpak()
        ? QStringLiteral("flatpak run ") + QString::fromUtf8(qgetenv("FLATPAK_ID"))
        : isRunningAsAppImage()
          ? QString::fromUtf8(qgetenv("APPIMAGE"))
          : QCoreApplication::applicationFilePath();
    content.replace(QStringLiteral("@EXEC@"), exec);

    // Write the XDG autostart entry.
    const QString autostartDir = configDir + QStringLiteral("/autostart");
    const QString desktopFile  = autostartDir + QStringLiteral("/proton-vpn-gui.desktop");

    QDir().mkpath(autostartDir);

    QFile outFile(desktopFile);
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        outFile.write(content.toUtf8());
    }
    else
    {
        DBG_APP(QStringLiteral("Migration: failed to write XDG autostart file: ") + outFile.errorString());
    }
}

} // namespace Migrations

// ---------------------------------------------------------------------------
// Debug-only test helpers
// ---------------------------------------------------------------------------

#ifdef QT_DEBUG

QString Migrations::testMigrateSystemdToXdgAutostart(const QString& simulatedPreviousVersion)
{
    const QVersionNumber prev = QVersionNumber::fromString(simulatedPreviousVersion);

    QStringList log;
    auto logLine = [&log](const QString& line)
    {
        log << line;
        DBG_APP(line);
    };

    DBG_APP(QStringLiteral("[TEST] migrateSystemdToXdgAutostart"));
    logLine(QStringLiteral("Simulated previous version: '%1'  (parsed: %2)")
            .arg(simulatedPreviousVersion,
                 prev.isNull() ? QStringLiteral("(null/invalid)") : prev.toString()));

    if (prev.isNull())
    {
        logLine(QStringLiteral("→ SKIP: null version treated as a fresh install"));
        return log.join(QLatin1Char('\n'));
    }
    if (prev > LAST_SYSTEMD_VERSION)
    {
        logLine(QStringLiteral("→ SKIP: %1 > %2 (already past migration point)")
                .arg(prev.toString(), LAST_SYSTEMD_VERSION.toString()));
        return log.join(QLatin1Char('\n'));
    }

    const QString configDir   = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString serviceFile = configDir + QStringLiteral("/systemd/user/proton-vpn-gui.service");

    logLine(QStringLiteral("Checking for service file: %1").arg(serviceFile));

    if (QFileInfo::exists(serviceFile) == false)
    {
        logLine(QStringLiteral("→ SKIP: service file not found (auto-start was not enabled under old version)"));
        return log.join(QLatin1Char('\n'));
    }

    logLine(QStringLiteral("Service file found."));
    logLine(QStringLiteral("→ WOULD remove: %1").arg(serviceFile));
    logLine(QStringLiteral("→ WOULD write XDG autostart entry: %1/autostart/proton-vpn-gui.desktop").arg(configDir));

    return log.join(QLatin1Char('\n'));
}

QString Migrations::testAll(const QString& simulatedPreviousVersion)
{
    DBG_APP(QStringLiteral("[TEST] Running all migrations with simulated previous version: '%1'")
            .arg(simulatedPreviousVersion));
    QStringList parts;
    parts << (QStringLiteral("=== migrateSystemdToXdgAutostart ===\n")
              + testMigrateSystemdToXdgAutostart(simulatedPreviousVersion));
    return parts.join(QStringLiteral("\n\n"));
}

#endif // QT_DEBUG
