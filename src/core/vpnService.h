#pragma once
// vpnService.h
// VpnService - the composition root of vpncore and the single entry point
// the UI talks to. Owns the CLI client, the state machine, the status
// poller, and the NAT-PMP service, and orchestrates flows that span several
// CLI calls (login, auto-connect with retries, disconnect-then-reconnect).
//
// All signals carry typed payloads from cliTypes.h. The Phase-1 widgets UI
// consumes this through the thin VpnManager façade; the Phase-2 QML UI will
// consume it directly.

#include "cliClient.h"
#include "cliTypes.h"
#include "natPmpService.h"
#include "processRunner.h"
#include "statusPoller.h"
#include "vpnStateMachine.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <functional>

class VpnService final : public QObject
{
    Q_OBJECT

public:
    // runner == nullptr creates a production QProcessRunner; tests inject a
    // FakeProcessRunner.
    explicit VpnService(ProcessRunner* runner = nullptr, QObject* parent = nullptr);

    // Sub-services -------------------------------------------------------

    ProtonVpnCliClient* cli()    const { return m_client; }
    NatPmpService*      natPmp() const { return m_natPmp; }

    // State accessors ----------------------------------------------------

    VpnState    state()              const { return m_stateMachine->state(); }
    QString     connectedServer()    const { return m_stateMachine->connectedServer(); }
    // Latest `protonvpn status` snapshot seen (ip/protocol/etc. in .raw).
    const StatusSnapshot& lastSnapshot() const { return m_lastSnapshot; }
    AccountType accountType()        const { return m_accountType; }
    QString     lastConnectCountry() const { return m_lastConnectCountry; }
    QString     lastConnectCity()    const { return m_lastConnectCity; }
    bool        isLoginInProgress()  const;
    bool        portForwardingEnabled() const;

    // Install / auth -----------------------------------------------------

    void checkInstalled();
    void checkLoginStatus();
    void login(const QString& username, const QString& password);
    void cancelLogin();
    void submit2fa(const QString& token);
    void signOut();

    // Connection ---------------------------------------------------------

    void connectVpn(const QString& country = QString(), const QString& city = QString());

    // Startup auto-connect: waits for NetworkManager to report a ready
    // state, then connects with retry/backoff.
    void startupAutoConnect(const QString& country = QString(), const QString& city = QString());

    void disconnectVpn();

    // Non-blocking replacement for the old synchronous quit-path disconnect:
    // runs `protonvpn disconnect` and invokes done() when it finishes or
    // after timeoutMs, whichever comes first.
    void disconnectThen(const std::function<void()>& done, int timeoutMs);

    // Config -------------------------------------------------------------

    void applyConfigValue(const QString& key, const QString& value);
    void applyConfigValueAndReconnect(const QString& key, const QString& value);

    // Queries ------------------------------------------------------------

    void fetchCountries();
    void fetchCities(const QString& countryCode);
    void fetchCityFeatures(const QString& countryCode, const QString& city,
                           const std::function<void(const QString& features)>& callback);
    void fetchInfo();
    void fetchSettings();
    void fetchCliVersion();
    void fetchAccountType();

signals:
    void installedResult(bool installed);
    void loginStatusResult(bool loggedIn, const QString& username);
    void twoFactorRequired();
    void loginFinished(bool ok, const QString& error);
    void signOutFinished(bool ok);

    void stateChanged(VpnState state, const QString& info);
    void connectionCityKnown(const QString& city);
    void connectionCountryKnown(const QString& countryCode);
    // Every successful status poll (connection details like ip/protocol).
    void snapshotChanged(const StatusSnapshot& snapshot);

    void countriesReady(const QList<Country>& countries);
    void citiesReady(const QString& countryCode, const QList<City>& cities);
    void infoReady(const QMap<QString, QString>& info);
    void settingsReady(const QMap<QString, QString>& settings);
    void configApplied(const QString& output);
    void cliVersionReady(const QString& version);
    void accountTypeReady(AccountType type);
    void errorOccurred(const QString& error);

private:
    void checkLoginStatus(int retriesLeft);
    void checkNetworkReady(int retriesLeft, const std::function<void()>& onReady);
    void issueConnect(const QString& country, const QString& city, int retriesLeft);
    void startPolling();
    void stopPolling();

    ProcessRunner*      m_runner;
    ProtonVpnCliClient* m_client;
    VpnStateMachine*    m_stateMachine;
    StatusPoller*       m_poller;
    NatPmpService*      m_natPmp;

    LoginSession*  m_loginSession = nullptr;
    AccountType    m_accountType  = AccountType::Unknown;
    QString        m_lastConnectCountry;
    QString        m_lastConnectCity;
    StatusSnapshot m_lastSnapshot;
};
