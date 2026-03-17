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
    static AppConfig &instance();

    bool autoConnect() const;
    bool notifications() const;
    int  recentConnectionsCount() const;

    void setAutoConnect(bool value);
    void setNotifications(bool value);
    void setRecentConnectionsCount(int value);

private:
    AppConfig();
    void load();
    bool save() const;

    bool m_autoConnect    = false;
    bool m_notifications  = true;
    int  m_recentConnectionsCount = 5;
};

