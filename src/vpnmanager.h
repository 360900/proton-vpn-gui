#pragma once

#include <QProcess>
#include <QString>
#include <QMap>

class StatusMonitor; // cli/statusmonitor.h — forward-declared to keep this
                     // header lightweight; full type used only in vpnmanager.cpp.

enum class VpnState
{
    Unknown,
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
};

enum class AccountType
{
    Unknown,
    Free,
    Plus
};

class VpnManager : public QObject
{
    Q_OBJECT

public:
    explicit VpnManager(QObject* parent = nullptr);

    void checkInstalled();
    void checkLoginStatus();
    void login(const QString& username, const QString& password);
    void submit2FA(const QString& token) const;
    void signOut();
    void connectVpn(const QString& country = QString(), const QString& city = QString());
    void disconnectVpn();
    static void disconnectVpnSync(); // blocking disconnect — safe to call just before app exit
    // Disconnect, change any config key/value, then reconnect to the previous location.
    void applyConfigValueAndReconnect(const QString& key, const QString& value);
    void fetchCountries();
    void fetchCities(const QString& countryCode);
    void fetchInfo();
    void fetchSettings();
    void applyConfig(const QString& key, bool enabled);
    void applyConfigValue(const QString& key, const QString& value);
    // Async: fetch the features string for a specific city in a country.
    // Parses `protonvpn cities list <countryCode>` and calls back with the
    // features string (e.g. "P2P, Tor") or an empty string if not found.
    void fetchCityFeatures(const QString& countryCode, const QString& city,
                           std::function<void(const QString& features)> callback);
    void fetchCliVersion();
    void fetchAccountType();

    VpnState    currentState()       const { return m_state; }
    AccountType accountType()        const { return m_accountType; }
    // Last country / city passed to connectVpn() — empty if connected via CLI.
    QString     lastConnectCountry() const { return m_lastConnectCountry; }
    QString     lastConnectCity()    const { return m_lastConnectCity; }
    // Reads the port-forwarding setting directly from the settings JSON file.
    bool        portForwardingEnabled() const;

signals:
    void installedResult(bool installed);
    void loginStatusResult(bool loggedIn, const QString& username);
    void twoFactorRequired();
    void loginFinished(bool ok, const QString& error);
    void signOutFinished(bool ok);
    void connectionStateChanged(VpnState state, const QString& info);
    // Emitted (before connectionStateChanged) when a city is parsed from
    // `protonvpn status` output, so the UI can pre-select it in the picker.
    void connectionCityKnown(const QString& city);
    // Emitted alongside connectionCityKnown with the 2-letter country code
    // extracted from the connected server name (e.g. "US" from "US-NJ#189").
    void connectionCountryKnown(const QString& countryCode);
    void countriesReady(const QMap<QString, QString>& countries); // name → code
    void citiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities); // (city, features)
    void infoReady(const QMap<QString, QString>& info);
    void settingsReady(const QMap<QString, QString>& settings);
    void configApplied(const QString& output);
    void cliVersionReady(const QString& version);
    void accountTypeReady(AccountType type);
    void errorOccurred(const QString& error);

private:
    VpnState    m_state         = VpnState::Unknown;
    AccountType m_accountType   = AccountType::Unknown;
    QString     m_connectedServer;       // last server string seen while Connected
    QString     m_lastConnectCountry;    // country arg last passed to connectVpn()
    QString     m_lastConnectCity;       // city    arg last passed to connectVpn()
    QProcess*       m_signinProcess  = nullptr;
    StatusMonitor*  m_statusMonitor  = nullptr;

    void runCommand(const QStringList& args,
                    std::function<void(int exitCode, const QString& output, const QString& errOutput)> callback);

    // Background status monitor (long-lived subprocess, every 15 s while logged in).
    void startStatusMonitor();
    void stopStatusMonitor();

    // Apply a parsed `protonvpn status` snapshot to internal state and emit
    // the appropriate signals.  Only emits when state or connected server
    // actually changed, to avoid unnecessary UI redraws.
    void applyStatusFields(const QMap<QString, QString>& fields);
};
