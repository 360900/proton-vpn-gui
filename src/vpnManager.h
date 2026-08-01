#pragma once
// vpnManager.h
// Thin façade over core/vpnService.h, preserving the signal/method surface
// the QtWidgets pages were written against. All logic lives in vpncore;
// this class only converts typed payloads into the legacy QMap/QPair shapes.
//
// The Phase-2 QML UI will talk to VpnService directly and this façade will
// be deleted together with the widgets pages.

#include "core/cliTypes.h"
#include "core/natPmpService.h"
#include "core/vpnService.h"

#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QString>
#include <functional>

class VpnManager : public QObject
{
    Q_OBJECT

public:
    explicit VpnManager(QObject* parent = nullptr);

    void checkInstalled();
    void checkLoginStatus();
    void login(const QString& username, const QString& password);
    void cancelLogin();
    void submit2FA(const QString& token) const;
    void signOut();
    void connectVpn(const QString& country = QString(), const QString& city = QString());
    void startupAutoConnect(const QString& country = QString(), const QString& city = QString());
    void disconnectVpn();

    // Non-blocking replacement for the old disconnectVpnSync(): disconnects
    // and invokes done() when finished or after timeoutMs, whichever first.
    void disconnectThen(const std::function<void()>& done, int timeoutMs);

    void applyConfigValueAndReconnect(const QString& key, const QString& value);
    void fetchCountries();
    void fetchCities(const QString& countryCode);
    void fetchInfo();
    void fetchSettings();
    void applyConfig(const QString& key, bool enabled);
    void applyConfigValue(const QString& key, const QString& value);
    void fetchCityFeatures(const QString& countryCode, const QString& city,
                           const std::function<void(const QString& features)>& callback);
    void fetchCliVersion();
    void fetchAccountType();

    VpnState    currentState()       const { return m_service->state(); }
    bool        isLoginInProgress()  const { return m_service->isLoginInProgress(); }
    AccountType accountType()        const { return m_service->accountType(); }
    QString     lastConnectCountry() const { return m_service->lastConnectCountry(); }
    QString     lastConnectCity()    const { return m_service->lastConnectCity(); }
    QString     connectedServer()    const { return m_service->connectedServer(); }
    bool        portForwardingEnabled() const { return m_service->portForwardingEnabled(); }

    // The NAT-PMP keep-alive service (owned by the underlying VpnService).
    NatPmpService* natPmp() const { return m_service->natPmp(); }

    // The underlying service - the object the D-Bus adaptors attach to.
    VpnService* service() const { return m_service; }

signals:
    void installedResult(bool installed);
    void loginStatusResult(bool loggedIn, const QString& username);
    void twoFactorRequired();
    void loginFinished(bool ok, const QString& error);
    void signOutFinished(bool ok);
    void connectionStateChanged(VpnState state, const QString& info);
    void connectionCityKnown(const QString& city);
    void connectionCountryKnown(const QString& countryCode);
    void countriesReady(const QMap<QString, QString>& countries); // name -> code
    void citiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities); // (city, features)
    void infoReady(const QMap<QString, QString>& info);
    void settingsReady(const QMap<QString, QString>& settings);
    void configApplied(const QString& output);
    void cliVersionReady(const QString& version);
    void accountTypeReady(AccountType type);
    void errorOccurred(const QString& error);

private:
    VpnService* m_service;
};
