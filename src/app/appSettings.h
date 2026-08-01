#pragma once
// appSettings.h
// QML-facing application preferences: wraps AppConfig with bindable
// properties, plus the XDG autostart entry and history/favorites clearing.

#include <QObject>
#include <QQmlEngine>
#include <QString>

class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY changed)
    Q_PROPERTY(QString autoConnectServer READ autoConnectServer WRITE setAutoConnectServer NOTIFY changed)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY changed)
    Q_PROPERTY(int recentConnectionsCount READ recentConnectionsCount WRITE setRecentConnectionsCount NOTIFY changed)
    Q_PROPERTY(bool startHidden READ startHidden WRITE setStartHidden NOTIFY changed)
    Q_PROPERTY(bool checkForUpdates READ checkForUpdates WRITE setCheckForUpdates NOTIFY changed)
    Q_PROPERTY(bool logToFile READ logToFile WRITE setLogToFile NOTIFY changed)
    Q_PROPERTY(bool sidebarCollapsed READ sidebarCollapsed WRITE setSidebarCollapsed NOTIFY changed)
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY changed)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY changed)
    Q_PROPERTY(QString autoStartError READ autoStartError NOTIFY changed)

public:
    static AppSettings* instance();
    static AppSettings* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    bool autoConnect() const;
    void setAutoConnect(bool value);
    QString autoConnectServer() const;
    void setAutoConnectServer(const QString& value);
    bool notifications() const;
    void setNotifications(bool value);
    int recentConnectionsCount() const;
    void setRecentConnectionsCount(int value);
    bool startHidden() const;
    void setStartHidden(bool value);
    bool checkForUpdates() const;
    void setCheckForUpdates(bool value);
    bool logToFile() const;
    void setLogToFile(bool value);
    bool sidebarCollapsed() const;
    void setSidebarCollapsed(bool value);
    bool reduceMotion() const;
    void setReduceMotion(bool value);

    // XDG autostart entry (written to the host config dir under Flatpak).
    bool autoStart() const;
    void setAutoStart(bool enable);
    QString autoStartError() const { return m_autoStartError; }

    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void clearFavorites();
    Q_INVOKABLE bool hasHistory() const;
    Q_INVOKABLE bool hasFavorites() const;
    Q_INVOKABLE bool isFavorite(const QString& countryCode, const QString& city) const;
    Q_INVOKABLE void addFavorite(const QString& countryCode, const QString& city);
    Q_INVOKABLE void removeFavorite(const QString& countryCode, const QString& city);
    Q_INVOKABLE void toggleFavorite(const QString& countryCode, const QString& city);

signals:
    void changed();

private:
    explicit AppSettings(QObject* parent = nullptr);
    static QString autoStartFilePath();

    QString m_autoStartError;
};
