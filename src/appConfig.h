#pragma once

#include <QString>
#include <QMap>

// ---------------------------------------------------------------------------
// AppConfig – persists app-level preferences to
//   ~/.config/ProtonVPN-Qt/app.json
// ---------------------------------------------------------------------------
class AppConfig
{
public:
    enum class Theme { System, Dark, Light };

    static AppConfig &instance();

    // Logs every loaded setting via DBG_SETTINGS. Split out from load() so
    // callers can control exactly when it prints relative to other startup
    // diagnostics (see main.cpp).
    void logLoadedConfig() const;

    bool autoConnect() const;
    // Empty string means "Fastest Server"; "CC" means fastest in country;
    // "CC|city" means a specific city.  Stored as-is.
    QString autoConnectServer() const;
    bool notifications() const;
    int  recentConnectionsCount() const;
    bool startHidden() const;
    Theme theme() const;
    bool showLocationPicker() const;
    bool showFavoritesDropdown() const;
    bool favoritesEnabled() const;
    QString lastSeenVersion() const;
    bool checkForUpdates() const;
    bool logToFile() const;

    void setAutoConnect(bool value);
    void setAutoConnectServer(const QString& value);
    void setNotifications(bool value);
    void setRecentConnectionsCount(int value);
    void setStartHidden(bool value);
    void setTheme(Theme value);
    void setShowLocationPicker(bool value);
    void setShowFavoritesDropdown(bool value);
    void setFavoritesEnabled(bool value);
    void setLastSeenVersion(const QString& value);
    void setCheckForUpdates(bool value);
    void setLogToFile(bool value);

    // Resets every setting to its compile-time default and deletes the config
    // file. The in-memory state is usable immediately; the file will not be
    // recreated until the next call to a setter (which triggers save()).
    void resetToDefaults();

private:
    AppConfig();
    void load();
    bool save() const;

    bool m_autoConnect    = false;
    QString m_autoConnectServer;
    bool m_notifications  = true;
    int  m_recentConnectionsCount = 5;
    bool m_startHidden = false;
    Theme m_theme = Theme::System;
    bool m_showLocationPicker = true;
    bool m_showFavoritesDropdown = true;
    bool m_favoritesEnabled = true;
    QString m_lastSeenVersion;
    bool m_checkForUpdates = true;
    bool m_logToFile = false;
};
