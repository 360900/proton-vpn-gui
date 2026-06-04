#include "migrations.h"
#include "cli/flatpakutils.h"
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
} // namespace

namespace Migrations
{

void run(const QString& previousVersion)
{
    migrateSystemdToXdgAutostart(previousVersion);
}

void migrateSystemdToXdgAutostart(const QString& previousVersion)
{
    // The last version that used a systemd user service for auto-start.
    // Versions after this switched to an XDG autostart .desktop file.
    const QVersionNumber LAST_SYSTEMD_VERSION(1, 9, 0);

    const QVersionNumber prev = QVersionNumber::fromString(previousVersion);
    // Skip on a fresh install (empty previousVersion) or if already past the
    // migration point.
    if (prev.isNull() || prev > LAST_SYSTEMD_VERSION)
        return;

    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    const QString serviceFile = configDir + QStringLiteral("/systemd/user/proton-vpn-qt.service");

    // If the service file does not exist, auto-start was not enabled under the
    // old version — remove nothing, create nothing.
    if (QFileInfo::exists(serviceFile) == false)
        return;

    DBG_APP(QStringLiteral("Migration: replacing legacy systemd unit with XDG autostart entry"));

    if constexpr (DRY_RUN_MODE == true)
    {
        DBG_APP(QStringLiteral("[DRY RUN] Would remove: ") + serviceFile);
        DBG_APP(QStringLiteral("[DRY RUN] Would write XDG autostart entry"));
        return;
    }

    QFile::remove(serviceFile);

    // Load the bundled .desktop template.
    QFile templateFile(QStringLiteral(":/autostart/proton-vpn-qt.desktop"));
    if (templateFile.open(QIODevice::ReadOnly) == false)
    {
        DBG_APP(QStringLiteral("Migration: failed to read autostart template resource"));
        return;
    }
    QString content = QString::fromUtf8(templateFile.readAll());
    templateFile.close();

    // Substitute the executable path placeholder.
    const QString exec = isRunningAsFlatpak()
        ? QStringLiteral("flatpak run ") + QString::fromUtf8(qgetenv("FLATPAK_ID"))
        : QCoreApplication::applicationFilePath();
    content.replace(QStringLiteral("@EXEC@"), exec);

    // Write the XDG autostart entry.
    const QString autostartDir = configDir + QStringLiteral("/autostart");
    const QString desktopFile  = autostartDir + QStringLiteral("/proton-vpn-qt.desktop");

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
