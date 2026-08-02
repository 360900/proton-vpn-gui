// cliClient.cpp
// See cliClient.h.

#include "cliClient.h"

#include "debug.h"

#include <QRegularExpression>

namespace
{
constexpr char CLI_PROGRAM[] = "protonvpn"; // NOLINT(*-avoid-c-arrays)

// Per-verb timeouts. Generous, because the CLI may refresh its server list
// or negotiate a tunnel before answering; a stuck process must still never
// hang the app indefinitely.
constexpr int TIMEOUT_HELP_MS       = 10'000;
constexpr int TIMEOUT_INFO_MS       = 15'000;
constexpr int TIMEOUT_STATUS_MS     = 20'000;
constexpr int TIMEOUT_LIST_MS       = 30'000;
constexpr int TIMEOUT_CONFIG_MS     = 20'000;
constexpr int TIMEOUT_CONNECT_MS    = 60'000;
constexpr int TIMEOUT_DISCONNECT_MS = 30'000;
constexpr int TIMEOUT_SIGNOUT_MS    = 15'000;
constexpr int TIMEOUT_VERSION_MS    = 10'000;
} // namespace

// ---------------------------------------------------------------------------
// LoginSession
// ---------------------------------------------------------------------------

LoginSession::LoginSession(ProcessHandle* handle, const QString& password, QObject* parent)
    : QObject(parent)
    , m_handle(handle)
    , m_password(password)
{
    connect(m_handle, &ProcessHandle::outputReceived, this, &LoginSession::onOutput);
    connect(m_handle, &ProcessHandle::finished, this,
            [this](const int exitCode, const QString& combinedOutput)
            {
                if (m_canceled)
                {
                    return;
                }
                m_password.fill(QLatin1Char('\0'));
                const LoginResult result = CliParsers::parseLoginOutput(exitCode, combinedOutput);
                DBG_CLI(result.ok ? QStringLiteral("Login succeeded.")
                                  : QStringLiteral("Login failed (exit=%1).").arg(exitCode));
                emit finished(result);
                deleteLater();
            });
}

void LoginSession::onOutput(const QString& chunk)
{
    m_accumulated += chunk;

    if (m_passwordSent == false && m_accumulated.contains(QStringLiteral("Password:")))
    {
        m_passwordSent = true;
        m_handle->writeStdin((m_password + QLatin1Char('\n')).toUtf8());
    }

    if (m_twoFaEmitted == false && m_accumulated.contains(QStringLiteral("2FA Token:")))
    {
        m_twoFaEmitted = true;
        DBG_CLI(QStringLiteral("Two-factor authentication required."));
        emit twoFactorRequired();
    }
}

void LoginSession::submit2fa(const QString& token)
{
    if (isRunning())
    {
        m_handle->writeStdin((token + QLatin1Char('\n')).toUtf8());
    }
}

void LoginSession::cancel()
{
    m_canceled = true;
    m_password.fill(QLatin1Char('\0'));
    m_handle->kill();
    deleteLater();
}

bool LoginSession::isRunning() const
{
    return m_handle->isRunning();
}

// ---------------------------------------------------------------------------
// ProtonVpnCliClient
// ---------------------------------------------------------------------------

ProtonVpnCliClient::ProtonVpnCliClient(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner)
{
}

void ProtonVpnCliClient::run(const QStringList& args, const int timeoutMs,
                             const ProcessRunner::Callback& done)
{
    const QString cmdLine = QString::fromLatin1(CLI_PROGRAM) + QLatin1Char(' ')
                            + args.join(QLatin1Char(' '));
    DBG_CLI(QStringLiteral(">>> ") + cmdLine);
    m_runner->run(QString::fromLatin1(CLI_PROGRAM), args, timeoutMs,
                  [cmdLine, done](const ProcessRunner::Result& result)
                  {
                      DBG_CLI(QStringLiteral("<<< %1 [exit=%2%3]")
                                  .arg(cmdLine)
                                  .arg(result.exitCode)
                                  .arg(result.timedOut ? QStringLiteral(", TIMED OUT")
                                       : result.failedToStart ? QStringLiteral(", FAILED TO START")
                                                              : QString()));
                      done(result);
                  });
}

void ProtonVpnCliClient::checkInstalled(const std::function<void(bool)>& done)
{
    run({QStringLiteral("--help")}, TIMEOUT_HELP_MS,
        [done](const ProcessRunner::Result& r)
        {
            // Under Flatpak, flatpak-spawn itself starts fine even when the
            // host has no protonvpn binary; the failure arrives as
            // "Failed to start command" on stderr.
            const bool spawnFailed = r.stdErr.contains(QStringLiteral("Failed to start command"));
            const bool installed = r.failedToStart == false && spawnFailed == false &&
                                   (r.stdOut.isEmpty() == false || r.stdErr.isEmpty() == false);
            done(installed);
        });
}

void ProtonVpnCliClient::accountName(
    const std::function<void(bool cliResponded, const QString& account)>& done)
{
    run({QStringLiteral("info")}, TIMEOUT_INFO_MS,
        [done](const ProcessRunner::Result& r)
        {
            const QMap<QString, QString> info = CliParsers::parseInfoMap(r.stdOut);
            if (info.contains(QStringLiteral("Account")) == false)
            {
                done(false, QString());
                return;
            }
            const QString account = info.value(QStringLiteral("Account"));
            done(true, account == QStringLiteral("None") ? QString() : account);
        });
}

LoginSession* ProtonVpnCliClient::signin(const QString& username, const QString& password)
{
    DBG_CLI(QStringLiteral("Login attempt for user: ") + username);
    ProcessHandle* handle = m_runner->startInteractive(
        QString::fromLatin1(CLI_PROGRAM),
        {QStringLiteral("signin"), username},
        /*detachFromTty=*/true);
    return new LoginSession(handle, password, this);
}

void ProtonVpnCliClient::signout(const std::function<void(bool)>& done)
{
    run({QStringLiteral("signout")}, TIMEOUT_SIGNOUT_MS,
        [done](const ProcessRunner::Result& r)
        {
            done(r.ok());
        });
}

void ProtonVpnCliClient::connectTo(const QString& country, const QString& city,
                                   const std::function<void(bool, const QString&,
                                                            const QString&)>& done)
{
    QStringList args{QStringLiteral("connect")};
    if (country.isEmpty() == false)
    {
        args << QStringLiteral("--country") << country;
    }
    if (city.isEmpty() == false)
    {
        args << QStringLiteral("--city") << city;
    }

    run(args, TIMEOUT_CONNECT_MS,
        [done](const ProcessRunner::Result& r)
        {
            if (r.ok())
            {
                done(true, CliParsers::stripNoise(r.stdOut), QString());
            }
            else
            {
                const QString rawError = r.stdErr.trimmed().isEmpty()
                    ? r.stdOut.trimmed()
                    : r.stdErr.trimmed();
                const QString error = CliParsers::stripNoise(rawError).trimmed();
                done(false, QString(), error.isEmpty()
                                      ? QStringLiteral("The Proton VPN CLI failed without an error message.")
                                      : error);
            }
        });
}

void ProtonVpnCliClient::disconnect(const std::function<void(bool, const QString&,
                                                             const QString&)>& done)
{
    run({QStringLiteral("disconnect")}, TIMEOUT_DISCONNECT_MS,
        [done](const ProcessRunner::Result& r)
        {
            if (r.ok())
            {
                done(true, r.stdOut.trimmed(), QString());
            }
            else
            {
                const QString err = r.stdErr.trimmed().isEmpty() ? r.stdOut.trimmed()
                                                                 : r.stdErr.trimmed();
                done(false, QString(), err);
            }
        });
}

void ProtonVpnCliClient::status(const std::function<void(bool, const StatusSnapshot&)>& done)
{
    run({QStringLiteral("status")}, TIMEOUT_STATUS_MS,
        [done](const ProcessRunner::Result& r)
        {
            done(r.ok(), CliParsers::parseStatus(r.stdOut));
        });
}

void ProtonVpnCliClient::countries(const std::function<void(bool, const QList<Country>&)>& done)
{
    run({QStringLiteral("countries"), QStringLiteral("list")}, TIMEOUT_LIST_MS,
        [done](const ProcessRunner::Result& r)
        {
            // Parse stdout only: stderr carries unrelated Python-runtime
            // warnings that must never be misread as table rows. Fall back
            // to stderr only when stdout is empty.
            const QString text = r.stdOut.trimmed().isEmpty() ? r.stdErr : r.stdOut;
            done(r.ok(), CliParsers::parseCountriesTable(text));
        });
}

void ProtonVpnCliClient::cities(const QString& countryCode,
                                const std::function<void(bool, const QList<City>&)>& done)
{
    run({QStringLiteral("cities"), QStringLiteral("list"), countryCode}, TIMEOUT_LIST_MS,
        [done](const ProcessRunner::Result& r)
        {
            const QString text = r.stdOut.trimmed().isEmpty() ? r.stdErr : r.stdOut;
            done(r.ok(), CliParsers::parseCitiesTable(text));
        });
}

void ProtonVpnCliClient::info(const std::function<void(const QMap<QString, QString>&)>& done)
{
    run({QStringLiteral("info")}, TIMEOUT_INFO_MS,
        [done](const ProcessRunner::Result& r)
        {
            done(CliParsers::parseInfoMap(r.stdOut));
        });
}

void ProtonVpnCliClient::accountTier(const std::function<void(AccountType)>& done)
{
    run({QStringLiteral("config"), QStringLiteral("list")}, TIMEOUT_CONFIG_MS,
        [done](const ProcessRunner::Result& r)
        {
            done(CliParsers::parseAccountTier(r.stdOut + QLatin1Char('\n') + r.stdErr));
        });
}

void ProtonVpnCliClient::cliVersion(const std::function<void(const QString&)>& done)
{
    // The CLI prints its banner (containing the version) to stderr when run
    // with no arguments. Prefer a match on stdout should it ever move there.
    run({}, TIMEOUT_VERSION_MS,
        [done](const ProcessRunner::Result& r)
        {
            done(CliParsers::parseCliVersion(r.stdErr + QLatin1Char('\n') + r.stdOut));
        });
}

void ProtonVpnCliClient::configSet(const QString& key, const QStringList& values,
                                   const std::function<void(const QString&)>& done)
{
    DBG_CLI(QStringLiteral("Applying CLI config: %1 = %2").arg(key, values.join(QLatin1Char(' '))));
    QStringList args{QStringLiteral("config"), QStringLiteral("set"), key};
    args << values;
    run(args, TIMEOUT_CONFIG_MS,
        [done](const ProcessRunner::Result& r)
        {
            done((r.stdOut + QLatin1Char('\n') + r.stdErr).trimmed());
        });
}
