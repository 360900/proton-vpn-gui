// appSettings.cpp
// See appSettings.h.

#include "appSettings.h"

#include "../appConfig.h"
#include "../connectionHistory.h"
#include "../core/debug.h"
#include "../core/fileLogger.h"
#include "../core/hostCommand.h"
#include "../favoritesManager.h"
#include "../geoUtils.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

namespace
{
AppSettings* s_instance = nullptr;

// org.freedesktop.portal.Background - used to request autostart without
// needing --filesystem=xdg-config/autostart (which Flathub lint rejects).
constexpr auto PORTAL_BUS      = "org.freedesktop.portal.Desktop";
constexpr auto PORTAL_PATH     = "/org/freedesktop/portal/desktop";
constexpr auto PORTAL_INTERFACE = "org.freedesktop.portal.Background";
constexpr char PORTAL_METHOD[] = "RequestBackground";

// Fire a portal autostart request and (optimistically) set the mirror so the
// toggle reflects quickly. The portal applies the request asynchronously.
void portalRequestAutoStart(const bool enable)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(PORTAL_BUS), QString::fromLatin1(PORTAL_PATH),
        QString::fromLatin1(PORTAL_INTERFACE), QString::fromLatin1(PORTAL_METHOD));

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"),
                   QString::fromLatin1("pvg_") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    options.insert(QStringLiteral("reason"),
                   QStringLiteral("Launch Proton VPN GUI at login."));
    options.insert(QStringLiteral("autostart"), enable);

    msg << QString() /* parent_window */ << QVariant(options);
    // fire-and-forget: the portal reply is not needed to reflect the toggle
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
}
} // namespace

AppSettings* AppSettings::instance()
{
    if (s_instance == nullptr)
    {
        s_instance = new AppSettings();
    }
    return s_instance;
}

AppSettings* AppSettings::create(QQmlEngine*, QJSEngine*)
{
    AppSettings* inst = instance();
    QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
    return inst;
}

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
{
    connect(&ConnectionHistory::instance(), &ConnectionHistory::changed,
            this, &AppSettings::changed);
    connect(&FavoritesManager::instance(), &FavoritesManager::changed,
            this, &AppSettings::changed);
}

bool AppSettings::autoConnect() const { return AppConfig::instance().autoConnect(); }
void AppSettings::setAutoConnect(const bool value)
{
    AppConfig::instance().setAutoConnect(value);
    emit changed();
}

QString AppSettings::autoConnectServer() const { return AppConfig::instance().autoConnectServer(); }
void AppSettings::setAutoConnectServer(const QString& value)
{
    AppConfig::instance().setAutoConnectServer(value);
    emit changed();
}

bool AppSettings::notifications() const { return AppConfig::instance().notifications(); }
void AppSettings::setNotifications(const bool value)
{
    AppConfig::instance().setNotifications(value);
    emit changed();
}

int AppSettings::recentConnectionsCount() const
{
    return AppConfig::instance().recentConnectionsCount();
}
void AppSettings::setRecentConnectionsCount(const int value)
{
    AppConfig::instance().setRecentConnectionsCount(value);
    ConnectionHistory::instance().trimToCount(value);
    emit changed();
}

bool AppSettings::startHidden() const { return AppConfig::instance().startHidden(); }
void AppSettings::setStartHidden(const bool value)
{
    AppConfig::instance().setStartHidden(value);
    emit changed();
}

bool AppSettings::checkForUpdates() const { return AppConfig::instance().checkForUpdates(); }
void AppSettings::setCheckForUpdates(const bool value)
{
    AppConfig::instance().setCheckForUpdates(value);
    emit changed();
}

bool AppSettings::logToFile() const { return AppConfig::instance().logToFile(); }
void AppSettings::setLogToFile(const bool value)
{
    AppConfig::instance().setLogToFile(value);
    FileLogger::instance().setEnabled(value);
    emit changed();
}

bool AppSettings::sidebarCollapsed() const { return AppConfig::instance().sidebarCollapsed(); }
void AppSettings::setSidebarCollapsed(const bool value)
{
    AppConfig::instance().setSidebarCollapsed(value);
    emit changed();
}

bool AppSettings::reduceMotion() const { return AppConfig::instance().reduceMotion(); }
void AppSettings::setReduceMotion(const bool value)
{
    AppConfig::instance().setReduceMotion(value);
    emit changed();
}

// ---------------------------------------------------------------------------
// Autostart
// ---------------------------------------------------------------------------

QString AppSettings::autoStartFilePath()
{
    // Native installs write the real ~/.config/autostart desktop file.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return configDir + QStringLiteral("/autostart/proton-vpn-gui.desktop");
}

bool AppSettings::autoStart() const
{
    if (isRunningAsFlatpak())
    {
        // The portal owns the autostart entry on the host; we mirror what we
        // last requested so the toggle is stable across launches.
        return AppConfig::instance().autoStart();
    }
    return QFileInfo::exists(autoStartFilePath());
}

void AppSettings::setAutoStart(const bool enable)
{
    m_autoStartError.clear();

    if (isRunningAsFlatpak())
    {
        // Ask the host portal to start/stop us at login. The portal applies
        // the request asynchronously; the mirror is updated optimistically.
        portalRequestAutoStart(enable);
        AppConfig::instance().setAutoStart(enable);
        emit changed();
        return;
    }

    const QString filePath = autoStartFilePath();

    if (enable)
    {
        QFile templateFile(QStringLiteral(":/autostart/proton-vpn-gui.desktop"));
        if (templateFile.open(QIODevice::ReadOnly) == false)
        {
            m_autoStartError = tr("Could not read the autostart template resource.");
            emit changed();
            return;
        }
        QString content = QString::fromUtf8(templateFile.readAll());

        // The AppImage runtime mounts the bundle under /tmp/.mount_XXXX which
        // disappears on unmount; the persistent $APPIMAGE path survives reboot.
        const QString exec = isRunningAsAppImage()
            ? QString::fromUtf8(qgetenv("APPIMAGE"))
            : QCoreApplication::applicationFilePath();
        content.replace(QStringLiteral("@EXEC@"), exec);

        QDir targetDir = QFileInfo(filePath).dir();
        if (targetDir.mkpath(targetDir.absolutePath()) == false)
        {
            m_autoStartError = tr("Could not create the autostart directory.");
            emit changed();
            return;
        }

        QFile outFile(filePath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate) == false)
        {
            m_autoStartError = tr("Could not write the autostart file: %1")
                                   .arg(outFile.errorString());
            emit changed();
            return;
        }
        outFile.write(content.toUtf8());
        DBG_SETTINGS(QStringLiteral("Autostart entry written: ") + filePath);
    }
    else
    {
        if (QFileInfo::exists(filePath) && QFile::remove(filePath) == false)
        {
            m_autoStartError = tr("Could not remove the autostart file: %1").arg(filePath);
        }
        else
        {
            DBG_SETTINGS(QStringLiteral("Autostart entry removed: ") + filePath);
        }
    }
    emit changed();
}

// ---------------------------------------------------------------------------
// History / favorites
// ---------------------------------------------------------------------------

void AppSettings::clearHistory()
{
    ConnectionHistory::instance().clear();
}

void AppSettings::clearFavorites()
{
    FavoritesManager::instance().clear();
}

bool AppSettings::hasHistory() const
{
    return ConnectionHistory::instance().hasAnyEntries();
}

bool AppSettings::hasFavorites() const
{
    return FavoritesManager::instance().hasAnyEntries();
}

bool AppSettings::isFavorite(const QString& countryCode, const QString& city) const
{
    return FavoritesManager::instance().isFavorite(countryCode, city);
}

void AppSettings::addFavorite(const QString& countryCode, const QString& city)
{
    const QString name = GeoUtils::countryCodeToName(countryCode);
    if (name.isEmpty()) return;
    FavoritesManager::instance().add(countryCode, name, city);
}

void AppSettings::removeFavorite(const QString& countryCode, const QString& city)
{
    FavoritesManager::instance().remove(countryCode, city);
}

void AppSettings::toggleFavorite(const QString& countryCode, const QString& city)
{
    const QString name = GeoUtils::countryCodeToName(countryCode);
    if (name.isEmpty()) return;
    FavoritesManager::instance().toggle(countryCode, name, city);
}
