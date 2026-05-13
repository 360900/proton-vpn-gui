#include "appconfig.h"
#include "debug.h"

#include <QDir>
#include <QFile>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>

// ── Easy-to-change config location ──────────────────────────────────────────
static const QString kConfigDir = QDir::homePath() + QStringLiteral("/.config/ProtonVPN-Qt");
static const QString kConfigFile = kConfigDir + QStringLiteral("/app.json");
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
    QFile f(kConfigFile);
    if (!f.open(QIODevice::ReadOnly))
        return; // file doesn't exist yet — all values stay at defaults

    const QJsonObject obj = QJsonDocument::fromJson(f.readAll()).object();
    f.close();

    m_autoConnect = obj.value(QStringLiteral("auto_connect")).toBool(false);
    m_notifications = obj.value(QStringLiteral("notifications")).toBool(true);
    m_recentConnectionsCount = obj.value(QStringLiteral("recent_connections_count")).toInt(5);
    m_startHidden = obj.value(QStringLiteral("start_hidden")).toBool(false);

    DBG_SETTINGS(QStringLiteral("Config loaded from: ") + kConfigFile);
    DBG_SETTINGS(QStringLiteral("  auto_connect             = ") + (m_autoConnect ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  notifications            = ") + (m_notifications ? QStringLiteral("true") : QStringLiteral("false")));
    DBG_SETTINGS(QStringLiteral("  recent_connections_count = ") + QString::number(m_recentConnectionsCount));
    DBG_SETTINGS(QStringLiteral("  start_hidden             = ") + (m_startHidden ? QStringLiteral("true") : QStringLiteral("false")));
}

bool AppConfig::save() const
{
    const QDir dir;
    if (!dir.mkpath(kConfigDir))
        return false;

    QJsonObject obj;
    obj[QStringLiteral("auto_connect")] = m_autoConnect;
    obj[QStringLiteral("notifications")] = m_notifications;
    obj[QStringLiteral("recent_connections_count")] = m_recentConnectionsCount;
    obj[QStringLiteral("start_hidden")] = m_startHidden;

    QFile f(kConfigFile);
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

