#include "appconfig.h"
#include "debug.h"

#include <QDir>
#include <QFile>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QStandardPaths>

// ── Easy-to-change config location ──────────────────────────────────────────
// QStandardPaths::GenericConfigLocation resolves to:
//   - Native install : ~/.config/ProtonVPN-Qt/
//   - Flatpak sandbox: ~/.var/app/io.github.wheat32.ProtonVPNQt/config/ProtonVPN-Qt/
// Using this instead of a hardcoded QDir::homePath() path means the Flatpak
// sandbox XDG remapping is honoured automatically with no extra --filesystem
// permission required.
static QString configDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/ProtonVPN-Qt");
}
static QString configFile() { return configDir() + QStringLiteral("/app.json"); }
// ────────────────────────────────────────────────────────────────────────────

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
    if (!f.open(QIODevice::ReadOnly))
        return; // file doesn't exist yet — all values stay at defaults

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    m_autoConnect = obj.value(QStringLiteral("auto_connect")).toBool(false);
    m_notifications = obj.value(QStringLiteral("notifications")).toBool(true);
    m_recentConnectionsCount = obj.value(QStringLiteral("recent_connections_count")).toInt(5);
    m_startHidden = obj.value(QStringLiteral("start_hidden")).toBool(false);
    m_showLocationPicker = obj.value(QStringLiteral("show_location_picker")).toBool(true);
    m_lastSeenVersion = obj.value(QStringLiteral("last_seen_version")).toString();

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

    DBG_SETTINGS(QStringLiteral("Config loaded from: ") + configFile());
    DBG_SETTINGS(QStringLiteral("  auto_connect             = ") + (m_autoConnect ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  notifications            = ") + (m_notifications ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  recent_connections_count = ") + QString::number(m_recentConnectionsCount));
    DBG_SETTINGS(QStringLiteral("  start_hidden             = ") + (m_startHidden ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  show_location_picker     = ") + (m_showLocationPicker ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  theme                    = ") + themeStr);
}

bool AppConfig::save() const
{
    const QDir dir;
    if (!dir.mkpath(configDir()))
        return false;

    QJsonObject obj;
    obj[QStringLiteral("auto_connect")] = m_autoConnect;
    obj[QStringLiteral("notifications")] = m_notifications;
    obj[QStringLiteral("recent_connections_count")] = m_recentConnectionsCount;
    obj[QStringLiteral("start_hidden")] = m_startHidden;
    obj[QStringLiteral("show_location_picker")] = m_showLocationPicker;
    if (m_lastSeenVersion.isEmpty() == false)
    {
        obj[QStringLiteral("last_seen_version")] = m_lastSeenVersion;
    }

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
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool AppConfig::autoConnect() const { return m_autoConnect; }

void AppConfig::setAutoConnect(bool value)
{
    if (m_autoConnect == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: auto_connect = ") + (value ? QStringLiteral("true") : QStringLiteral("false")));
    m_autoConnect = value;
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

QString AppConfig::lastSeenVersion() const { return m_lastSeenVersion; }

void AppConfig::setLastSeenVersion(const QString& value)
{
    if (m_lastSeenVersion == value) return;
    DBG_SETTINGS(QStringLiteral("Setting changed: last_seen_version = ") + value);
    m_lastSeenVersion = value;
    (void)save();
}

void AppConfig::resetToDefaults()
{
    DBG_SETTINGS(QStringLiteral("AppConfig::resetToDefaults() — deleting config file and resetting all values"));

    // Delete the persisted file first so no stale data remains on disk.
    QFile::remove(configFile());

    // Reset every member to its compile-time default.
    m_autoConnect            = false;
    m_notifications          = true;
    m_recentConnectionsCount = 5;
    m_startHidden            = false;
    m_theme                  = Theme::System;
    m_showLocationPicker     = true;
    m_lastSeenVersion        = QString();
}

