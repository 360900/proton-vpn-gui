#pragma once

#include <QString>
#include <QMap>

// ---------------------------------------------------------------------------
// AppConfig – persists app-level preferences to
//   ~/.config/ProtonVPN-Qt/app.json
//
// Easy to relocate: change kConfigDir / kConfigFile below.
// ---------------------------------------------------------------------------
class AppConfig
{
public:
    // Returns the shared instance (loaded on first access).
    static AppConfig &instance();

    // Getters
    bool autoConnect() const;
    bool notifications() const;

    // Setters – immediately persist to disk.
    void setAutoConnect(bool value);
    void setNotifications(bool value);

private:
    AppConfig();  // use instance()

    void load();
    bool save() const;

    bool m_autoConnect    = false;
    bool m_notifications  = true;
};

