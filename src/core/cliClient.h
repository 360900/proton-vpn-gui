#pragma once
// cliClient.h
// ProtonVpnCliClient - typed asynchronous interface to the `protonvpn` CLI.
// One method per CLI verb; raw output never leaves this layer un-parsed
// (all interpretation happens in cliParsers).
//
// The client owns no state beyond in-flight processes; VPN state lives in
// VpnStateMachine / VpnService.

#include "cliParsers.h"
#include "cliTypes.h"
#include "processRunner.h"

#include <QObject>
#include <QString>
#include <functional>

// A running `protonvpn signin` flow. Created by ProtonVpnCliClient::signin();
// parented to the client. Feeds the password automatically when prompted and
// surfaces the 2FA prompt to the UI.
class LoginSession final : public QObject
{
    Q_OBJECT

public:
    LoginSession(ProcessHandle* handle, const QString& password, QObject* parent);

    void submit2fa(const QString& token);
    void cancel();
    bool isRunning() const;

signals:
    void twoFactorRequired();
    void finished(const LoginResult& result);

private:
    void onOutput(const QString& chunk);

    ProcessHandle* m_handle;
    QString        m_password;
    QString        m_accumulated;
    bool           m_passwordSent = false;
    bool           m_twoFaEmitted = false;
    bool           m_canceled    = false;
};

class ProtonVpnCliClient final : public QObject
{
    Q_OBJECT

public:
    explicit ProtonVpnCliClient(ProcessRunner* runner, QObject* parent = nullptr);

    // Install / auth ------------------------------------------------------

    // Runs `protonvpn --help`. installed is false when the binary is missing
    // on the host (including the flatpak-spawn "Failed to start command" case).
    void checkInstalled(const std::function<void(bool installed)>& done);

    // Runs `protonvpn info` and reports the signed-in account name
    // (empty when signed out or when the CLI did not answer cleanly).
    // cliResponded distinguishes "signed out" from "CLI not ready yet".
    void accountName(const std::function<void(bool cliResponded, const QString& account)>& done);

    // Starts `protonvpn signin <username>`; the session auto-feeds the
    // password when prompted. Ownership: the returned session is parented to
    // this client and deletes itself after finishing.
    LoginSession* signin(const QString& username, const QString& password);

    void signout(const std::function<void(bool ok)>& done);

    // Connection ----------------------------------------------------------

    // `protonvpn connect [--country CC] [--city City]`.
    // cleanedMessage is the CLI's success text with noise stripped.
    void connectTo(const QString& country, const QString& city,
                   const std::function<void(bool ok, const QString& cleanedMessage,
                                            const QString& errorText)>& done);

    void disconnect(const std::function<void(bool ok, const QString& message,
                                             const QString& errorText)>& done);

    void status(const std::function<void(bool ok, const StatusSnapshot& snapshot)>& done);

    // Queries -------------------------------------------------------------

    void countries(const std::function<void(bool ok, const QList<Country>&)>& done);
    void cities(const QString& countryCode,
                const std::function<void(bool ok, const QList<City>&)>& done);
    void info(const std::function<void(const QMap<QString, QString>&)>& done);
    void accountTier(const std::function<void(AccountType)>& done);
    void cliVersion(const std::function<void(const QString& version)>& done);

    // Config --------------------------------------------------------------

    // `protonvpn config set <key> <value...>`; done receives combined output.
    void configSet(const QString& key, const QStringList& values,
                   const std::function<void(const QString& output)>& done);

private:
    void run(const QStringList& args, int timeoutMs, const ProcessRunner::Callback& done);

    ProcessRunner* m_runner;
};
