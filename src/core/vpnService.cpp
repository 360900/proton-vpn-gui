// vpnService.cpp
// See vpnService.h.

#include "vpnService.h"

#include "cliParsers.h"
#include "debug.h"
#include "protonCliSettings.h"

#include <QPointer>
#include <QTimer>

namespace
{
constexpr int LOGIN_CHECK_RETRIES         = 5;
constexpr int NETWORK_READY_CHECK_RETRIES = 5;
constexpr int AUTO_CONNECT_RETRIES        = 5;
constexpr int NMCLI_TIMEOUT_MS            = 5'000;
constexpr int RETRY_BACKOFF_STEP_MS       = 1'000;
} // namespace

VpnService::VpnService(ProcessRunner* runner, QObject* parent)
    : QObject(parent)
    , m_runner(runner != nullptr ? runner : new QProcessRunner(this))
    , m_client(new ProtonVpnCliClient(m_runner, this))
    , m_stateMachine(new VpnStateMachine(this))
    , m_poller(new StatusPoller(m_client, this))
    , m_natPmp(new NatPmpService(m_runner, this))
{
    // State machine -> outside world.
    connect(m_stateMachine, &VpnStateMachine::stateChanged, this,
            [this](const VpnState state, const QString& info)
            {
                // Fast poll cadence while a transition is in flight, so a
                // hung CLI command still resolves via status polls.
                m_poller->setTransitionMode(state == VpnState::Connecting ||
                                            state == VpnState::Disconnecting);
                emit stateChanged(state, info);
            });
    connect(m_stateMachine, &VpnStateMachine::connectionCityKnown,
            this, &VpnService::connectionCityKnown);
    connect(m_stateMachine, &VpnStateMachine::connectionCountryKnown,
            this, &VpnService::connectionCountryKnown);
    connect(m_stateMachine, &VpnStateMachine::forcePollRequested,
            m_poller, &StatusPoller::pollNow);

    // Poller -> state machine (and to consumers interested in the details).
    connect(m_poller, &StatusPoller::snapshotReady,
            m_stateMachine, &VpnStateMachine::applySnapshot);
    connect(m_poller, &StatusPoller::snapshotReady, this,
            [this](const StatusSnapshot& snapshot)
            {
                m_lastSnapshot = snapshot;
                emit snapshotChanged(snapshot);
            });
}

bool VpnService::isLoginInProgress() const
{
    return m_loginSession != nullptr && m_loginSession->isRunning();
}

bool VpnService::portForwardingEnabled() const
{
    return ProtonCliSettings::portForwardingEnabled();
}

// ---------------------------------------------------------------------------
// Install / auth
// ---------------------------------------------------------------------------

void VpnService::checkInstalled()
{
    DBG_CLI(QStringLiteral("Checking if protonvpn CLI is installed..."));
    m_client->checkInstalled([this](const bool installed)
    {
        DBG_CLI(installed ? QStringLiteral("protonvpn CLI found.")
                          : QStringLiteral("protonvpn CLI NOT found."));
        emit installedResult(installed);
    });
}

void VpnService::checkLoginStatus()
{
    checkLoginStatus(LOGIN_CHECK_RETRIES);
}

void VpnService::checkLoginStatus(const int retriesLeft)
{
    m_client->accountName([this, retriesLeft](const bool cliResponded, const QString& account)
    {
        if (cliResponded)
        {
            const bool loggedIn = account.isEmpty() == false;
            if (loggedIn)
            {
                startPolling();
                fetchAccountType();
            }
            emit loginStatusResult(loggedIn, account);
            return;
        }

        // The CLI didn't answer cleanly - transient on autostart while the
        // daemon/keyring is still coming up. Retry with backoff.
        DBG_CLI(QStringLiteral("checkLoginStatus: no Account field, retries left: %1")
                    .arg(retriesLeft));
        if (retriesLeft > 0)
        {
            const int attempt = LOGIN_CHECK_RETRIES - retriesLeft + 1;
            QTimer::singleShot(attempt * RETRY_BACKOFF_STEP_MS, this, [this, retriesLeft]
            {
                checkLoginStatus(retriesLeft - 1);
            });
        }
        else
        {
            emit loginStatusResult(false, QString());
        }
    });
}

void VpnService::login(const QString& username, const QString& password)
{
    cancelLogin();
    m_loginSession = m_client->signin(username, password);

    connect(m_loginSession, &LoginSession::twoFactorRequired,
            this, &VpnService::twoFactorRequired);
    connect(m_loginSession, &LoginSession::finished, this,
            [this](const LoginResult& result)
            {
                m_loginSession = nullptr;
                if (result.ok)
                {
                    startPolling();
                    fetchAccountType();
                }
                emit loginFinished(result.ok, result.errorText);
            });
    connect(m_loginSession, &QObject::destroyed, this, [this](QObject* obj)
            {
                if (obj == m_loginSession)
                {
                    m_loginSession = nullptr;
                }
            });
}

void VpnService::cancelLogin()
{
    if (m_loginSession != nullptr)
    {
        LoginSession* session = m_loginSession;
        m_loginSession = nullptr;
        session->cancel();
    }
}

void VpnService::submit2fa(const QString& token)
{
    if (m_loginSession != nullptr)
    {
        m_loginSession->submit2fa(token);
    }
}

void VpnService::signOut()
{
    DBG_CLI(QStringLiteral("Signing out..."));
    stopPolling();
    m_client->signout([this](const bool ok)
    {
        DBG_CLI(ok ? QStringLiteral("Sign-out succeeded.") : QStringLiteral("Sign-out failed."));
        m_stateMachine->reset(VpnState::Disconnected);
        emit signOutFinished(ok);
    });
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

void VpnService::connectVpn(const QString& country, const QString& city)
{
    DBG_CLI(QStringLiteral("Connecting to VPN - country: '%1'  city: '%2'")
                .arg(country.isEmpty() ? QStringLiteral("(fastest)") : country,
                     city.isEmpty() ? QStringLiteral("(any)") : city));
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;

    m_stateMachine->beginConnecting();
    issueConnect(country, city, 0);
}

void VpnService::startupAutoConnect(const QString& country, const QString& city)
{
    DBG_CLI(QStringLiteral("Auto-connecting to VPN on startup - country: '%1'  city: '%2'")
                .arg(country.isEmpty() ? QStringLiteral("(fastest)") : country,
                     city.isEmpty() ? QStringLiteral("(any)") : city));
    m_lastConnectCountry = country;
    m_lastConnectCity    = city;

    m_stateMachine->beginConnecting();
    checkNetworkReady(NETWORK_READY_CHECK_RETRIES, [this, country, city]
    {
        issueConnect(country, city, AUTO_CONNECT_RETRIES);
    });
}

void VpnService::checkNetworkReady(const int retriesLeft, const std::function<void()>& onReady)
{
    m_runner->run(QStringLiteral("nmcli"),
                  {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("STATE"),
                   QStringLiteral("general"), QStringLiteral("status")},
                  NMCLI_TIMEOUT_MS,
                  [this, retriesLeft, onReady](const ProcessRunner::Result& r)
                  {
                      // nmcli missing or broken: fail open rather than block
                      // auto-connect on systems without NetworkManager.
                      if (r.failedToStart || r.timedOut)
                      {
                          onReady();
                          return;
                      }

                      // Only the "connected*" family means a default route is
                      // up; startsWith so "disconnected" does not match.
                      const QString state = r.stdOut.trimmed();
                      const bool ready = r.exitCode == 0 &&
                                         state.startsWith(QStringLiteral("connected"));
                      if (ready || retriesLeft <= 0)
                      {
                          onReady();
                          return;
                      }

                      const int attempt = NETWORK_READY_CHECK_RETRIES - retriesLeft + 1;
                      DBG_CLI(QStringLiteral("Network not ready yet (nmcli state: '%1'), "
                                             "retrying in %2 ms...")
                                  .arg(state).arg(attempt * RETRY_BACKOFF_STEP_MS));
                      QTimer::singleShot(attempt * RETRY_BACKOFF_STEP_MS, this,
                                         [this, retriesLeft, onReady]
                      {
                          checkNetworkReady(retriesLeft - 1, onReady);
                      });
                  });
}

void VpnService::issueConnect(const QString& country, const QString& city, const int retriesLeft)
{
    m_client->connectTo(country, city,
                        [this, country, city, retriesLeft](const bool ok, const QString& message,
                                                           const QString& errorText)
    {
        if (ok)
        {
            DBG_CLI(QStringLiteral("VPN connected successfully."));
            const ServerInfo server = CliParsers::parseServerInfo(message);
            if (server.countryCode.isEmpty() == false)
            {
                emit connectionCountryKnown(server.countryCode);
            }
            // Keep Connecting until status confirms the target. The CLI can
            // return success before its status endpoint has updated, and a
            // stale disconnected snapshot must not undo a real connection.
            m_poller->pollNow();
            return;
        }

        if (retriesLeft > 0)
        {
            const int attempt = AUTO_CONNECT_RETRIES - retriesLeft + 1;
            DBG_CLI(QStringLiteral("VPN connect attempt failed, retrying in %1 ms...")
                        .arg(attempt * RETRY_BACKOFF_STEP_MS));
            QTimer::singleShot(attempt * RETRY_BACKOFF_STEP_MS, this,
                               [this, country, city, retriesLeft]
            {
                issueConnect(country, city, retriesLeft - 1);
            });
            return;
        }

        DBG_CLI(QStringLiteral("VPN connection failed: ") + errorText);
        m_stateMachine->connectFailed(errorText);
        emit errorOccurred(errorText);
    });
}

void VpnService::disconnectVpn()
{
    DBG_CLI(QStringLiteral("Disconnecting VPN..."));
    m_stateMachine->beginDisconnecting();

    m_client->disconnect([this](const bool ok, const QString& message, const QString& errorText)
    {
        if (ok)
        {
            DBG_CLI(QStringLiteral("VPN disconnected successfully."));
            m_stateMachine->disconnectSucceeded(message);
        }
        else
        {
            DBG_CLI(QStringLiteral("VPN disconnect failed: ") + errorText);
            m_stateMachine->disconnectFailed(errorText);
            emit errorOccurred(errorText);
        }
    });
}

void VpnService::disconnectThen(const std::function<void()>& done, const int timeoutMs)
{
    auto fired = std::make_shared<bool>(false);
    auto fireOnce = [fired, done]
    {
        if (*fired)
        {
            return;
        }
        *fired = true;
        done();
    };

    QTimer::singleShot(timeoutMs, this, fireOnce);
    m_client->disconnect([fireOnce](bool, const QString&, const QString&)
    {
        fireOnce();
    });
}

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

void VpnService::applyConfigValue(const QString& key, const QString& value)
{
    m_client->configSet(key, value.split(QLatin1Char(' '), Qt::SkipEmptyParts),
                        [this](const QString& output)
    {
        emit configApplied(output);
    });
}

void VpnService::applyConfigValueAndReconnect(const QString& key, const QString& value)
{
    const QString country = m_lastConnectCountry;
    const QString city    = m_lastConnectCity;

    m_stateMachine->beginDisconnecting();
    m_client->disconnect([this, key, value, country, city](const bool ok, const QString&,
                                                           const QString& errorText)
    {
        if (ok == false)
        {
            m_stateMachine->disconnectFailed(errorText);
            emit errorOccurred(errorText);
            return;
        }
        m_stateMachine->disconnectSucceeded(QString());

        m_client->configSet(key, value.split(QLatin1Char(' '), Qt::SkipEmptyParts),
                            [this, country, city](const QString& output)
        {
            emit configApplied(output);
            connectVpn(country, city);
        });
    });
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

void VpnService::fetchCountries()
{
    m_client->countries([this](const bool ok, const QList<Country>& countries)
    {
        if (ok)
        {
            emit countriesReady(countries);
        }
    });
}

void VpnService::fetchCities(const QString& countryCode)
{
    m_client->cities(countryCode, [this, countryCode](const bool ok, const QList<City>& cities)
    {
        if (ok) // don't overwrite a good list on auth/transient errors
        {
            emit citiesReady(countryCode, cities);
        }
    });
}

void VpnService::fetchCityFeatures(const QString& countryCode, const QString& city,
                                   const std::function<void(const QString&)>& callback)
{
    m_client->cities(countryCode, [city, callback](const bool ok, const QList<City>& cities)
    {
        if (ok == false)
        {
            return;
        }
        for (const City& c : cities)
        {
            if (c.name.compare(city, Qt::CaseInsensitive) == 0)
            {
                callback(c.features);
                return;
            }
        }
        callback(QString());
    });
}

void VpnService::fetchInfo()
{
    m_client->info([this](const QMap<QString, QString>& info)
    {
        emit infoReady(info);
    });
}

void VpnService::fetchSettings()
{
    emit settingsReady(ProtonCliSettings::readSettings());
}

void VpnService::fetchCliVersion()
{
    m_client->cliVersion([this](const QString& version)
    {
        emit cliVersionReady(version);
    });
}

void VpnService::fetchAccountType()
{
    m_client->accountTier([this](const AccountType type)
    {
        m_accountType = type;
        emit accountTypeReady(type);
    });
}

// ---------------------------------------------------------------------------
// Polling
// ---------------------------------------------------------------------------

void VpnService::startPolling()
{
    m_poller->start();
}

void VpnService::stopPolling()
{
    m_poller->stop();
}
