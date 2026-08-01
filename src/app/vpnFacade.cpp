// vpnFacade.cpp
// See vpnFacade.h.

#include "vpnFacade.h"

#include "../appConfig.h"
#include "../connectionHistory.h"
#include "../core/debug.h"
#include "../geoUtils.h"

#include <QCoreApplication>

namespace
{
VpnFacade* s_instance = nullptr;

constexpr int DISCONNECT_QUIT_TIMEOUT_MS = 10'000;
} // namespace

VpnFacade* VpnFacade::instance()
{
    if (s_instance == nullptr)
    {
        s_instance = new VpnFacade();
    }
    return s_instance;
}

VpnFacade* VpnFacade::create(QQmlEngine* qmlEngine, QJSEngine*)
{
    VpnFacade* inst = instance();
    // main.cpp owns the singleton; the QML engine must not delete it.
    QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
    Q_UNUSED(qmlEngine)
    return inst;
}

VpnFacade::VpnFacade(QObject* parent)
    : QObject(parent)
    , m_service(new VpnService(nullptr, this))
{
    wireService();
}

void VpnFacade::wireService()
{
    connect(m_service, &VpnService::installedResult, this, [this](const bool installed)
    {
        if (installed)
        {
            m_service->checkLoginStatus();
        }
        else
        {
            setUiState(UiState::NotInstalled);
        }
    });

    connect(m_service, &VpnService::loginStatusResult, this,
            [this](const bool loggedIn, const QString& username)
    {
        m_username = username;
        emit usernameChanged();
        if (loggedIn)
        {
            setUiState(UiState::Main);
            m_service->fetchCountries();
            m_service->fetchSettings();
            m_service->fetchInfo();

            // Startup auto-connect ("" = fastest, "CC" = country, "CC|city").
            if (AppConfig::instance().autoConnect())
            {
                const QString stored = AppConfig::instance().autoConnectServer();
                const int sep = stored.indexOf(QLatin1Char('|'));
                const QString country = sep >= 0 ? stored.left(sep) : stored;
                const QString city    = sep >= 0 ? stored.mid(sep + 1) : QString();
                m_service->startupAutoConnect(country, city);
            }
        }
        else
        {
            setUiState(UiState::Login);
        }
    });

    connect(m_service, &VpnService::twoFactorRequired, this, [this]
    {
        m_twoFactorPending = true;
        emit twoFactorPendingChanged();
    });

    connect(m_service, &VpnService::loginFinished, this,
            [this](const bool ok, const QString& error)
    {
        m_loginBusy        = false;
        m_twoFactorPending = false;
        emit loginBusyChanged();
        emit twoFactorPendingChanged();
        if (ok)
        {
            m_loginError.clear();
            emit loginErrorChanged();
            setUiState(UiState::Main);
            m_service->checkLoginStatus(); // resolves username + account type
            m_service->fetchCountries();
            m_service->fetchSettings();
        }
        else
        {
            m_loginError = error;
            emit loginErrorChanged();
        }
    });

    connect(m_service, &VpnService::signOutFinished, this, [this](const bool ok)
    {
        Q_UNUSED(ok)
        m_username.clear();
        emit usernameChanged();
        setUiState(UiState::Login);
    });

    connect(m_service, &VpnService::stateChanged, this,
            [this](const VpnState state, const QString& info)
    {
        m_stateInfo = info;

        // The CLI reports auth expiry through connection errors - detect and
        // route to the login screen (parity with the old MainWindow sniffing).
        if (state == VpnState::Error)
        {
            m_lastError = info;
            emit lastErrorChanged();
            handleAuthError(info);
        }

        if (state == VpnState::Connected)
        {
            if (m_connectedSince.isValid() == false)
            {
                m_connectedSince = QDateTime::currentDateTime();
            }
            // Port forwarding keep-alive while connected.
            if (m_service->portForwardingEnabled())
            {
                m_service->natPmp()->refresh();
            }
        }
        else
        {
            m_connectedSince = QDateTime();
            if (state == VpnState::Disconnected || state == VpnState::Error)
            {
                m_connectedCity.clear();
                m_connectedCountryCode.clear();
                m_connectionTargetCountryCode.clear();
                emit connectionTargetChanged();
            }
            m_service->natPmp()->stop();
            emit forwardedPortChanged();
        }

        emit connStateChanged();
        emit connectionDetailsChanged();
    });

    connect(m_service, &VpnService::connectionCityKnown, this, [this](const QString& city)
    {
        m_connectedCity = city;
        emit connectionDetailsChanged();
    });
    connect(m_service, &VpnService::connectionCountryKnown, this, [this](const QString& cc)
    {
        m_connectedCountryCode = cc;
        m_connectionTargetCountryCode = cc;
        emit connectionTargetChanged();
        emit connectionDetailsChanged();

        // Record history now that the full location is known.
        if (m_service->state() == VpnState::Connected)
        {
            ConnectionHistory::instance().record(cc, GeoUtils::countryCodeToName(cc),
                                                 m_connectedCity);
            emit connected(cc, m_connectedCity);
        }
    });

    connect(m_service, &VpnService::snapshotChanged, this, [this](const StatusSnapshot&)
    {
        emit connectionDetailsChanged();
    });

    connect(m_service, &VpnService::accountTypeReady, this, [this](AccountType)
    {
        emit planChanged();
    });

    connect(m_service, &VpnService::infoReady, this,
            [this](const QMap<QString, QString>& info)
    {
        const QString account = info.value(QStringLiteral("Account"));
        if (account.isEmpty() == false && account != QStringLiteral("None"))
        {
            m_username = account;
            emit usernameChanged();
        }
    });

    connect(m_service, &VpnService::settingsReady, this,
            [this](const QMap<QString, QString>& settings)
    {
        QVariantMap map;
        for (auto it = settings.constBegin(); it != settings.constEnd(); ++it)
        {
            map.insert(it.key(), it.value());
        }
        emit settingsChanged(map);
    });

    connect(m_service, &VpnService::configApplied, this, &VpnFacade::configApplied);
    connect(m_service, &VpnService::countriesReady, this, &VpnFacade::countriesReady);
    connect(m_service, &VpnService::citiesReady, this, &VpnFacade::citiesReady);

    connect(m_service, &VpnService::cliVersionReady, this, [this](const QString& version)
    {
        m_cliVersion = version;
        emit cliVersionChanged();
    });

    connect(m_service->natPmp(), &NatPmpService::portAcquired, this,
            [this](int) { emit forwardedPortChanged(); });
    connect(m_service->natPmp(), &NatPmpService::portLost, this,
            [this] { emit forwardedPortChanged(); });

    connect(&ConnectionHistory::instance(), &ConnectionHistory::changed,
            this, &VpnFacade::lastLocationChanged);
}

// ---------------------------------------------------------------------------
// Getters with logic
// ---------------------------------------------------------------------------

QString VpnFacade::ipAddress() const
{
    return m_service->lastSnapshot().raw.value(QStringLiteral("ip"));
}

QString VpnFacade::protocol() const
{
    return m_service->lastSnapshot().raw.value(QStringLiteral("protocol"));
}

VpnFacade::Plan VpnFacade::plan() const
{
    switch (m_service->accountType())
    {
        case AccountType::Free:
            return Plan::Free;
        case AccountType::Plus:
            return Plan::Paid;
        default:
            return Plan::Unknown;
    }
}

int VpnFacade::forwardedPort() const
{
    return m_service->natPmp()->forwardedPort();
}

QString VpnFacade::appVersion() const
{
    return QCoreApplication::applicationVersion();
}

QString VpnFacade::lastLocationCountryCode() const
{
    const auto entries = ConnectionHistory::instance().entries();
    return entries.isEmpty() ? QString() : entries.first().countryCode;
}

QString VpnFacade::lastLocationCountryName() const
{
    const auto entries = ConnectionHistory::instance().entries();
    return entries.isEmpty() ? QString() : entries.first().countryName;
}

QString VpnFacade::lastLocationCity() const
{
    const auto entries = ConnectionHistory::instance().entries();
    return entries.isEmpty() ? QString() : entries.first().city;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void VpnFacade::startup()
{
    setUiState(UiState::Loading);
    m_service->fetchCliVersion();
    m_service->checkInstalled();
}

void VpnFacade::login(const QString& username, const QString& password)
{
    m_loginBusy = true;
    m_loginError.clear();
    emit loginBusyChanged();
    emit loginErrorChanged();
    m_service->login(username, password);
}

void VpnFacade::submit2fa(const QString& token)
{
    m_service->submit2fa(token);
}

void VpnFacade::cancelLogin()
{
    m_service->cancelLogin();
    m_loginBusy        = false;
    m_twoFactorPending = false;
    emit loginBusyChanged();
    emit twoFactorPendingChanged();
}

void VpnFacade::signOut()
{
    m_service->signOut();
}

void VpnFacade::recheckInstalled()
{
    setUiState(UiState::Loading);
    m_service->checkInstalled();
}

void VpnFacade::connectTo(const QString& countryCode, const QString& city)
{
    m_connectionTargetCountryCode = countryCode;
    emit connectionTargetChanged();
    m_service->connectVpn(countryCode, city);
}

void VpnFacade::connectFastest()
{
    m_connectionTargetCountryCode.clear();
    emit connectionTargetChanged();
    m_service->connectVpn(QString(), QString());
}

void VpnFacade::connectLastLocation()
{
    const QString cc = lastLocationCountryCode();
    if (cc.isEmpty() == false)
    {
        m_connectionTargetCountryCode = cc;
        emit connectionTargetChanged();
        m_service->connectVpn(cc, lastLocationCity());
    }
}

void VpnFacade::disconnect()
{
    m_service->disconnectVpn();
}

void VpnFacade::togglePower()
{
    const VpnState state = m_service->state();
    if (state == VpnState::Connected || state == VpnState::Connecting)
    {
        m_service->disconnectVpn();
    }
    else
    {
        m_service->connectVpn(m_service->lastConnectCountry(), m_service->lastConnectCity());
    }
}

void VpnFacade::refreshSettings()
{
    m_service->fetchSettings();
}

void VpnFacade::applyConfigValue(const QString& key, const QString& value)
{
    m_service->applyConfigValue(key, value);
}

void VpnFacade::applyConfigValueAndReconnect(const QString& key, const QString& value)
{
    m_service->applyConfigValueAndReconnect(key, value);
}

void VpnFacade::disconnectThenQuit()
{
    m_service->disconnectThen([] { QCoreApplication::quit(); }, DISCONNECT_QUIT_TIMEOUT_MS);
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

void VpnFacade::setUiState(const UiState state)
{
    if (m_uiState == state)
    {
        return;
    }
    m_uiState = state;
    emit uiStateChanged();
}

void VpnFacade::handleAuthError(const QString& info)
{
    const QString lower = info.toLower();
    const bool authError = lower.contains(QStringLiteral("authentication required")) ||
                           lower.contains(QStringLiteral("please sign in with")) ||
                           lower.contains(QStringLiteral("401"));
    if (authError == false)
    {
        return;
    }
    DBG_APP(QStringLiteral("Auth error detected in connection failure - signing out."));
    m_service->signOut();
}
