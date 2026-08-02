#include <QDir>
#include <QFile>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QStandardPaths>
#include "appConfig.h"
#include "debug.h"
#include "fileLogger.h"

namespace
{
// Easy-to-change config location
// QStandardPaths::GenericConfigLocation resolves to:
//   - Native install : ~/.config/ProtonVPN-GUI/
//   - Flatpak sandbox: ~/.var/app/io.github._360900.ProtonVpnGui/config/ProtonVPN-GUI/
QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/ProtonVPN-GUI");
}
QString configFile() { return configDir() + QStringLiteral("/app.json"); }

constexpr int DEFAULT_RECENT_CONNECTIONS_COUNT = 5;
} // namespace

AppConfig& AppConfig::instance()
{
    static AppConfig inst;
    return inst;
}

AppConfig::AppConfig()
{
    load();
}

void AppConfig::load()
{
    QFile f(configFile());
    if (f.open(QIODevice::ReadOnly) == false) return; // file doesn't exist yet - defaults

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    m_autoConnect = obj.value(QStringLiteral("auto_connect")).toBool(false);
    m_autoConnectServer = obj.value(QStringLiteral("auto_connect_server")).toString();
    m_notifications = obj.value(QStringLiteral("notifications")).toBool(true);
    m_recentConnectionsCount = obj.value(QStringLiteral("recent_connections_count")).toInt(DEFAULT_RECENT_CONNECTIONS_COUNT);
    m_startHidden = obj.value(QStringLiteral("start_hidden")).toBool(false);
    m_showLocationPicker = obj.value(QStringLiteral("show_location_picker")).toBool(true);
    m_showFavoritesDropdown = obj.value(QStringLiteral("show_favorites_dropdown")).toBool(true);
    m_favoritesEnabled = obj.value(QStringLiteral("favorites_enabled")).toBool(true);
    m_lastSeenVersion = obj.value(QStringLiteral("last_seen_version")).toString();
    m_checkForUpdates = obj.value(QStringLiteral("check_for_updates")).toBool(true);
    m_logToFile = obj.value(QStringLiteral("log_to_file")).toBool(false);
    m_sidebarCollapsed = obj.value(QStringLiteral("sidebar_collapsed")).toBool(false);
    m_reduceMotion = obj.value(QStringLiteral("reduce_motion")).toBool(false);

    const QString themeStr = obj.value(QStringLiteral("theme")).toString(QStringLiteral("system"));
    if (themeStr == QStringLiteral("dark"))
    {
        m_theme = Theme::Dark;
    }
    else if (themeStr == QStringLiteral("light"))
    {
        m_theme = Theme::Light;
    }
    else
    {
        m_theme = Theme::System;
    }
}

void AppConfig::logLoadedConfig() const
{
    QString themeStr;
    switch (m_theme)
    {
        case Theme::Dark:
            themeStr = QStringLiteral("dark");
            break;

        case Theme::Light:
            themeStr = QStringLiteral("light");
            break;

        default:
            themeStr = QStringLiteral("system");
            break;
    }

    DBG_SETTINGS(QStringLiteral("Config loaded from: ") + configFile());
    DBG_SETTINGS(QStringLiteral("  auto_connect             = ") + (m_autoConnect ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  auto_connect_server      = ") + m_autoConnectServer);
    DBG_SETTINGS(QStringLiteral("  notifications            = ") + (m_notifications ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  recent_connections_count = ") + QString::number(m_recentConnectionsCount));
    DBG_SETTINGS(QStringLiteral("  start_hidden             = ") + (m_startHidden ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  show_location_picker     = ") + (m_showLocationPicker ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  show_favorites_dropdown  = ") + (m_showFavoritesDropdown ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  favorites_enabled        = ") + (m_favoritesEnabled ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  last_seen_version        = ") + m_lastSeenVersion);
    DBG_SETTINGS(QStringLiteral("  check_for_updates        = ") + (m_checkForUpdates ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  log_to_file              = ") + (m_logToFile ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  sidebar_collapsed        = ") + (m_sidebarCollapsed ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  reduce_motion            = ") + (m_reduceMotion ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  theme                    = ") + themeStr);
}

bool AppConfig::save() const
{
    const QDir dir;
    if (dir.mkpath(configDir()) == false) return false;

    QJsonObject obj;
    obj[QStringLiteral("auto_connect")] = m_autoConnect;
    if (m_autoConnectServer.isEmpty() == false)
    {
        obj[QStringLiteral("auto_connect_server")] = m_autoConnectServer;
    }
    obj[QStringLiteral("notifications")] = m_notifications;
    obj[QStringLiteral("recent_connections_count")] = m_recentConnectionsCount;
    obj[QStringLiteral("start_hidden")] = m_startHidden;
    obj[QStringLiteral("show_location_picker")] = m_showLocationPicker;
    obj[QStringLiteral("show_favorites_dropdown")] = m_showFavoritesDropdown;
    obj[QStringLiteral("favorites_enabled")] = m_favoritesEnabled;
    if (m_lastSeenVersion.isEmpty() == false)
    {
        obj[QStringLiteral("last_seen_version")] = m_lastSeenVersion;
    }
    obj[QStringLiteral("check_for_updates")] = m_checkForUpdates;
    obj[QStringLiteral("log_to_file")] = m_logToFile;
    obj[QStringLiteral("sidebar_collapsed")] = m_sidebarCollapsed;
    obj[QStringLiteral("reduce_motion")] = m_reduceMotion;

    QString themeStr;
    switch (m_theme)
    {
        case Theme::Dark:
            themeStr = QStringLiteral("dark");
            break;
        case Theme::Light:
            themeStr = QStringLiteral("light");
            break;
        default:
            themeStr = QStringLiteral("system");
            break;
    }
    obj[QStringLiteral("theme")] = themeStr;

    QFile f(configFile());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text) == false)
        return false;

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool AppConfig::autoConnect() const { return m_autoConnect; }

QString AppConfig::autoConnectServer() const { return m_autoConnectServer; }

void AppConfig::setAutoConnect(const bool value)
{
    if (m_autoConnect == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: auto_connect = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_autoConnect = value;
    (void)save();
}

void AppConfig::setAutoConnectServer(const QString& value)
{
    if (m_autoConnectServer == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: auto_connect_server = ") + value);
    m_autoConnectServer = value;
    (void)save();
}

bool AppConfig::notifications() const { return m_notifications; }

void AppConfig::setNotifications(const bool value)
{
    if (m_notifications == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: notifications = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_notifications = value;
    (void)save();
}

int AppConfig::recentConnectionsCount() const { return m_recentConnectionsCount; }

void AppConfig::setRecentConnectionsCount(const int value)
{
    if (m_recentConnectionsCount == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: recent_connections_count = ") + QString::number(qMax(0, value)));
    m_recentConnectionsCount = qMax(0, value);
    (void)save();
}

bool AppConfig::startHidden() const { return m_startHidden; }

void AppConfig::setStartHidden(const bool value)
{
    if (m_startHidden == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: start_hidden = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_startHidden = value;
    (void)save();
}

AppConfig::Theme AppConfig::theme() const { return m_theme; }

void AppConfig::setTheme(const Theme value)
{
    if (m_theme == value) return;
    m_theme = value;
    (void)save();
}

bool AppConfig::showLocationPicker() const { return m_showLocationPicker; }

void AppConfig::setShowLocationPicker(const bool value)
{
    if (m_showLocationPicker == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: show_location_picker = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_showLocationPicker = value;
    (void)save();
}

bool AppConfig::showFavoritesDropdown() const { return m_showFavoritesDropdown; }

void AppConfig::setShowFavoritesDropdown(const bool value)
{
    if (m_showFavoritesDropdown == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: show_favorites_dropdown = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_showFavoritesDropdown = value;
    (void)save();
}

bool AppConfig::favoritesEnabled() const { return m_favoritesEnabled; }

void AppConfig::setFavoritesEnabled(const bool value)
{
    if (m_favoritesEnabled == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: favorites_enabled = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_favoritesEnabled = value;
    (void)save();
}

QString AppConfig::lastSeenVersion() const { return m_lastSeenVersion; }

void AppConfig::setLastSeenVersion(const QString& value)
{
    if (m_lastSeenVersion == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: last_seen_version = ") + value);
    m_lastSeenVersion = value;
    (void)save();
}

bool AppConfig::checkForUpdates() const { return m_checkForUpdates; }

void AppConfig::setCheckForUpdates(const bool value)
{
    if (m_checkForUpdates == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: check_for_updates = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_checkForUpdates = value;
    (void)save();
}

bool AppConfig::logToFile() const { return m_logToFile; }

void AppConfig::setLogToFile(const bool value)
{
    if (m_logToFile == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: log_to_file = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_logToFile = value;
    (void)save();
    FileLogger::instance().setEnabled(value);
}

bool AppConfig::sidebarCollapsed() const { return m_sidebarCollapsed; }

void AppConfig::setSidebarCollapsed(const bool value)
{
    if (m_sidebarCollapsed == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: sidebar_collapsed = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_sidebarCollapsed = value;
    (void)save();
}

bool AppConfig::reduceMotion() const { return m_reduceMotion; }

void AppConfig::setReduceMotion(const bool value)
{
    if (m_reduceMotion == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: reduce_motion = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_reduceMotion = value;
    (void)save();
}

void AppConfig::resetToDefaults()
{
    DBG_SETTINGS(QStringLiteral("AppConfig::resetToDefaults() - deleting config file and resetting all values"));

    // Delete the persisted file first so no stale data remains on disk.
    QFile::remove(configFile());

    // Reset every member to its compile-time default.
    m_autoConnect            = false;
    m_autoConnectServer      = QString();
    m_notifications          = true;
    m_recentConnectionsCount = DEFAULT_RECENT_CONNECTIONS_COUNT;
    m_startHidden            = false;
    m_theme                  = Theme::System;
    m_showLocationPicker     = true;
    m_showFavoritesDropdown  = true;
    m_favoritesEnabled       = true;
    m_lastSeenVersion        = QString();
    m_checkForUpdates        = true;
    m_logToFile              = false;
    m_sidebarCollapsed       = false;
    m_reduceMotion           = false;
    FileLogger::instance().setEnabled(false);
}

