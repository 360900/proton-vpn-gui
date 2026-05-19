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

    bool autoConnect() const;
    bool notifications() const;
    int  recentConnectionsCount() const;
    bool startHidden() const;
    Theme theme() const;
    bool showLocationPicker() const;

    void setAutoConnect(bool value);
    void setNotifications(bool value);
    void setRecentConnectionsCount(int value);
    void setStartHidden(bool value);
    void setTheme(Theme value);
    void setShowLocationPicker(bool value);

private:
    AppConfig();
    void load();
    bool save() const;

    bool m_autoConnect    = false;
    bool m_notifications  = true;
    int  m_recentConnectionsCount = 5;
    bool m_startHidden = false;
    Theme m_theme = Theme::System;
    bool m_showLocationPicker = true;
};

