// vpnManager.cpp
// See vpnManager.h - façade over core/vpnService.h.

#include "vpnManager.h"

VpnManager::VpnManager(QObject* parent)
    : QObject(parent)
    , m_service(new VpnService(nullptr, this))
{
    // Direct forwards.
    connect(m_service, &VpnService::installedResult,   this, &VpnManager::installedResult);
    connect(m_service, &VpnService::loginStatusResult, this, &VpnManager::loginStatusResult);
    connect(m_service, &VpnService::twoFactorRequired, this, &VpnManager::twoFactorRequired);
    connect(m_service, &VpnService::loginFinished,     this, &VpnManager::loginFinished);
    connect(m_service, &VpnService::signOutFinished,   this, &VpnManager::signOutFinished);
    connect(m_service, &VpnService::stateChanged,      this, &VpnManager::connectionStateChanged);
    connect(m_service, &VpnService::connectionCityKnown,
            this, &VpnManager::connectionCityKnown);
    connect(m_service, &VpnService::connectionCountryKnown,
            this, &VpnManager::connectionCountryKnown);
    connect(m_service, &VpnService::infoReady,        this, &VpnManager::infoReady);
    connect(m_service, &VpnService::settingsReady,    this, &VpnManager::settingsReady);
    connect(m_service, &VpnService::configApplied,    this, &VpnManager::configApplied);
    connect(m_service, &VpnService::cliVersionReady,  this, &VpnManager::cliVersionReady);
    connect(m_service, &VpnService::accountTypeReady, this, &VpnManager::accountTypeReady);
    connect(m_service, &VpnService::errorOccurred,    this, &VpnManager::errorOccurred);

    // Typed -> legacy payload conversions.
    connect(m_service, &VpnService::countriesReady, this,
            [this](const QList<Country>& countries)
            {
                QMap<QString, QString> map;
                for (const Country& c : countries)
                {
                    map.insert(c.name, c.code);
                }
                emit countriesReady(map);
            });
    connect(m_service, &VpnService::citiesReady, this,
            [this](const QString& countryCode, const QList<City>& cities)
            {
                QList<QPair<QString, QString>> pairs;
                pairs.reserve(cities.size());
                for (const City& c : cities)
                {
                    pairs.append({c.name, c.features});
                }
                emit citiesReady(countryCode, pairs);
            });
}

void VpnManager::checkInstalled()                { m_service->checkInstalled(); }
void VpnManager::checkLoginStatus()              { m_service->checkLoginStatus(); }
void VpnManager::cancelLogin()                   { m_service->cancelLogin(); }
void VpnManager::signOut()                       { m_service->signOut(); }
void VpnManager::disconnectVpn()                 { m_service->disconnectVpn(); }
void VpnManager::fetchCountries()                { m_service->fetchCountries(); }
void VpnManager::fetchInfo()                     { m_service->fetchInfo(); }
void VpnManager::fetchSettings()                 { m_service->fetchSettings(); }
void VpnManager::fetchCliVersion()               { m_service->fetchCliVersion(); }
void VpnManager::fetchAccountType()              { m_service->fetchAccountType(); }

void VpnManager::login(const QString& username, const QString& password)
{
    m_service->login(username, password);
}

void VpnManager::submit2FA(const QString& token) const
{
    m_service->submit2fa(token);
}

void VpnManager::connectVpn(const QString& country, const QString& city)
{
    m_service->connectVpn(country, city);
}

void VpnManager::startupAutoConnect(const QString& country, const QString& city)
{
    m_service->startupAutoConnect(country, city);
}

void VpnManager::disconnectThen(const std::function<void()>& done, const int timeoutMs)
{
    m_service->disconnectThen(done, timeoutMs);
}

void VpnManager::applyConfigValueAndReconnect(const QString& key, const QString& value)
{
    m_service->applyConfigValueAndReconnect(key, value);
}

void VpnManager::fetchCities(const QString& countryCode)
{
    m_service->fetchCities(countryCode);
}

void VpnManager::applyConfig(const QString& key, const bool enabled)
{
    applyConfigValue(key, enabled ? QStringLiteral("on") : QStringLiteral("off"));
}

void VpnManager::applyConfigValue(const QString& key, const QString& value)
{
    m_service->applyConfigValue(key, value);
}

void VpnManager::fetchCityFeatures(const QString& countryCode, const QString& city,
                                   const std::function<void(const QString&)>& callback)
{
    m_service->fetchCityFeatures(countryCode, city, callback);
}
