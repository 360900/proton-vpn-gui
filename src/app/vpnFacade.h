#pragma once
// vpnFacade.h
// VpnFacade - the QML-facing wrapper over core/vpnService.h. Exposes VPN
// state as bindable properties, forwards user actions, and orchestrates the
// startup flow (install check -> login check -> auto-connect).
//
// Registered as a QML singleton (import ProtonVpnGui; VpnFacade.xxx). The C++
// side (tray, D-Bus wiring in main.cpp) reaches it via instance().

#include "../core/cliTypes.h"
#include "../core/vpnService.h"

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

class VpnFacade : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // Which top-level screen the UI shows.
    enum class UiState { Loading, NotInstalled, Login, Main };
    Q_ENUM(UiState)

    // Mirror of core VpnState, registered for QML.
    enum class ConnState { Unknown, Disconnected, Connecting, Connected, Disconnecting, Error };
    Q_ENUM(ConnState)

    enum class Plan { Unknown, Free, Paid };
    Q_ENUM(Plan)

    Q_PROPERTY(UiState uiState READ uiState NOTIFY uiStateChanged)
    Q_PROPERTY(ConnState connState READ connState NOTIFY connStateChanged)
    Q_PROPERTY(QString stateInfo READ stateInfo NOTIFY connStateChanged)
    Q_PROPERTY(QString connectedServer READ connectedServer NOTIFY connectionDetailsChanged)
    Q_PROPERTY(QString connectedCity READ connectedCity NOTIFY connectionDetailsChanged)
    Q_PROPERTY(QString connectedCountryCode READ connectedCountryCode NOTIFY connectionDetailsChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY connectionDetailsChanged)
    Q_PROPERTY(QString protocol READ protocol NOTIFY connectionDetailsChanged)
    Q_PROPERTY(QDateTime connectedSince READ connectedSince NOTIFY connectionDetailsChanged)
    Q_PROPERTY(QString username READ username NOTIFY usernameChanged)
    Q_PROPERTY(Plan plan READ plan NOTIFY planChanged)
    Q_PROPERTY(bool loginBusy READ loginBusy NOTIFY loginBusyChanged)
    Q_PROPERTY(bool twoFactorPending READ twoFactorPending NOTIFY twoFactorPendingChanged)
    Q_PROPERTY(QString loginError READ loginError NOTIFY loginErrorChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int forwardedPort READ forwardedPort NOTIFY forwardedPortChanged)
    Q_PROPERTY(bool portForwardingEnabled READ portForwardingEnabled NOTIFY portForwardingEnabledChanged)
    Q_PROPERTY(QString cliVersion READ cliVersion NOTIFY cliVersionChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    // Most recent connection (persisted across restarts) - drives the
    // "Last location" quick action. Empty countryCode when no history.
    Q_PROPERTY(QString lastLocationCountryCode READ lastLocationCountryCode NOTIFY lastLocationChanged)
    Q_PROPERTY(QString lastLocationCountryName READ lastLocationCountryName NOTIFY lastLocationChanged)
    Q_PROPERTY(QString lastLocationCity READ lastLocationCity NOTIFY lastLocationChanged)
    Q_PROPERTY(QString connectionTargetCountryCode READ connectionTargetCountryCode NOTIFY connectionTargetChanged)

    static VpnFacade* instance();
    static VpnFacade* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    VpnService* service() const { return m_service; }

    // Property getters ----------------------------------------------------

    UiState   uiState() const { return m_uiState; }
    ConnState connState() const { return static_cast<ConnState>(m_service->state()); }
    QString   stateInfo() const { return m_stateInfo; }
    QString   connectedServer() const { return m_service->connectedServer(); }
    QString   connectedCity() const { return m_connectedCity; }
    QString   connectedCountryCode() const { return m_connectedCountryCode; }
    QString   ipAddress() const;
    QString   protocol() const;
    QDateTime connectedSince() const { return m_connectedSince; }
    QString   username() const { return m_username; }
    Plan      plan() const;
    bool      loginBusy() const { return m_loginBusy; }
    bool      twoFactorPending() const { return m_twoFactorPending; }
    QString   loginError() const { return m_loginError; }
    QString   lastError() const { return m_lastError; }
    int       forwardedPort() const;
    bool      portForwardingEnabled() const { return m_service->portForwardingEnabled(); }
    QString   cliVersion() const { return m_cliVersion; }
    QString   appVersion() const;
    QString   lastLocationCountryCode() const;
    QString   lastLocationCountryName() const;
    QString   lastLocationCity() const;
    QString   connectionTargetCountryCode() const { return m_connectionTargetCountryCode; }

    // Actions --------------------------------------------------------------

    Q_INVOKABLE void startup(); // install check -> login check -> main/login

    Q_INVOKABLE void login(const QString& username, const QString& password);
    Q_INVOKABLE void submit2fa(const QString& token);
    Q_INVOKABLE void cancelLogin();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void recheckInstalled();

    Q_INVOKABLE void connectTo(const QString& countryCode, const QString& city);
    Q_INVOKABLE void connectFastest();
    Q_INVOKABLE void connectLastLocation();
    Q_INVOKABLE void disconnect();
    // Connect if disconnected, disconnect if connected (tray / power button).
    Q_INVOKABLE void togglePower();

    Q_INVOKABLE void refreshSettings();
    Q_INVOKABLE void applyConfigValue(const QString& key, const QString& value);
    Q_INVOKABLE void applyConfigValueAndReconnect(const QString& key, const QString& value);

    // Non-blocking disconnect for the quit path (see MainWindow parity).
    Q_INVOKABLE void disconnectThenQuit();

signals:
    void uiStateChanged();
    void connStateChanged();
    void connectionDetailsChanged();
    void usernameChanged();
    void planChanged();
    void loginBusyChanged();
    void twoFactorPendingChanged();
    void loginErrorChanged();
    void lastErrorChanged();
    void forwardedPortChanged();
    void portForwardingEnabledChanged();
    void settingsChanged(const QVariantMap& settings);
    void cliVersionChanged();
    void lastLocationChanged();
    void connectionTargetChanged();
    void configApplied(const QString& output);
    void countriesReady(const QList<Country>& countries);
    void citiesReady(const QString& countryCode, const QList<City>& cities);
    void citiesFailed(const QString& countryCode);
    // Fired when a connection attempt succeeded (used to record history).
    void connected(const QString& countryCode, const QString& city);
    // In-app toast requests.
    void toast(const QString& message, bool success);

private:
    explicit VpnFacade(QObject* parent = nullptr);
    void setUiState(UiState state);
    void wireService();
    void handleAuthError(const QString& info);

    VpnService* m_service;
    UiState     m_uiState = UiState::Loading;
    QString     m_stateInfo;
    QString     m_connectedCity;
    QString     m_connectedCountryCode;
    QDateTime   m_connectedSince;
    QString     m_username;
    QString     m_loginError;
    QString     m_lastError;
    QString     m_cliVersion;
    QString     m_connectionTargetCountryCode;
    bool        m_loginBusy        = false;
    bool        m_twoFactorPending = false;
};
