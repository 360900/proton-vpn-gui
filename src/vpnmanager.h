#pragma once

#include <QProcess>
#include <QString>
#include <QMap>

enum class VpnState
{
    Unknown,
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Error
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
    void disconnectVpnSync(); // blocking disconnect — safe to call just before app exit
    void fetchCountries();
    void fetchCities(const QString& countryCode);
    void fetchInfo();
    void fetchSettings();
    void applyConfig(const QString& key, bool enabled);
    void applyConfigValue(const QString& key, const QString& value);
    void checkConnectionStatus();

    VpnState currentState() const { return m_state; }

signals:
    void installedResult(bool installed);
    void loginStatusResult(bool loggedIn, const QString& username);
    void twoFactorRequired();
    void loginFinished(bool ok, const QString& error);
    void signOutFinished(bool ok);
    void connectionStateChanged(VpnState state, const QString& info);
    void countriesReady(const QMap<QString, QString>& countries); // name → code
    void citiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities); // (city, features)
    void infoReady(const QMap<QString, QString>& info);
    void settingsReady(const QMap<QString, QString>& settings);
    void errorOccurred(const QString& error);

private:
    VpnState m_state = VpnState::Unknown;
    QProcess* m_signinProcess = nullptr;

    void runCommand(const QStringList& args,
                    std::function<void(int exitCode, const QString& output, const QString& errOutput)> callback);
    static QMap<QString, QString> parseDictOutput(const QString& output);
};
