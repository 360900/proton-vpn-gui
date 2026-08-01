#include <QtTest/QtTest>

#include "core/vpnService.h"
#include "support/fakeProcessRunner.h"

// VpnService orchestration tests over FakeProcessRunner: connect/disconnect
// flows drive the state machine, sign-in wiring, and query fan-out.

namespace
{
ProcessRunner::Result okResult(const QString& stdOut = QString())
{
    ProcessRunner::Result r;
    r.exitCode = 0;
    r.stdOut = stdOut;
    return r;
}

ProcessRunner::Result failResult(const QString& stdErr)
{
    ProcessRunner::Result r;
    r.exitCode = 1;
    r.stdErr = stdErr;
    return r;
}
} // namespace

class TstVpnService : public QObject
{
    Q_OBJECT

private slots:

    void connectVpn_success_reachesConnected()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(okResult(QStringLiteral("Connected to CH#7.\n")));
        // pollNow() after the connect issues a status call.
        runner.cannedResults.append(okResult(
            QStringLiteral("Status: Connected\nServer: CH#7 in Zurich, Switzerland\n")));

        VpnService service(&runner);
        QSignalSpy stateSpy(&service, &VpnService::stateChanged);
        QSignalSpy citySpy(&service, &VpnService::connectionCityKnown);

        service.connectVpn(QStringLiteral("CH"), QStringLiteral("Zurich"));

        QCOMPARE(service.state(), VpnState::Connected);
        QCOMPARE(service.connectedServer(), QStringLiteral("CH#7 in Zurich, Switzerland"));
        QCOMPARE(service.lastConnectCountry(), QStringLiteral("CH"));
        // Connecting -> Connected (command) -> server learned via poll.
        QVERIFY(stateSpy.count() >= 2);
        QCOMPARE(stateSpy.at(0).first().value<VpnState>(), VpnState::Connecting);
        QCOMPARE(citySpy.count(), 1);
        QCOMPARE(citySpy.first().first().toString(), QStringLiteral("Zurich"));
    }

    void connectVpn_failure_reachesError_andEmitsErrorOccurred()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(failResult(QStringLiteral("No servers available\n")));

        VpnService service(&runner);
        QSignalSpy errorSpy(&service, &VpnService::errorOccurred);

        service.connectVpn(QString(), QString());

        QCOMPARE(service.state(), VpnState::Error);
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.first().first().toString(), QStringLiteral("No servers available"));
    }

    void disconnectVpn_success_reachesDisconnected()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(okResult(QStringLiteral("Connected to CH#7.\n")));
        runner.cannedResults.append(okResult(
            QStringLiteral("Status: Connected\nServer: CH#7 in Zurich, Switzerland\n")));
        VpnService service(&runner);
        service.connectVpn(QString(), QString());

        runner.cannedResults.append(okResult(QStringLiteral("Disconnected.\n")));
        service.disconnectVpn();

        QCOMPARE(service.state(), VpnState::Disconnected);
        QVERIFY(service.connectedServer().isEmpty());
    }

    void disconnectThen_invokesCallbackOnce()
    {
        FakeProcessRunner runner;
        VpnService service(&runner);

        int calls = 0;
        service.disconnectThen([&] { ++calls; }, 1000);
        QCOMPARE(calls, 1);        // fake completes synchronously
        QTest::qWait(1100);        // the timeout fallback must not double-fire
        QCOMPARE(calls, 1);
    }

    void checkLoginStatus_signedIn_emitsUserAndStartsPolling()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(okResult(QStringLiteral("Account: 'user@example.com'\n")));
        // Polling starts on success -> first status poll.
        runner.cannedResults.append(okResult(QStringLiteral("Status: Disconnected\n")));
        // fetchAccountType -> config list.
        runner.cannedResults.append(okResult(QStringLiteral("netshield off\n")));

        VpnService service(&runner);
        QSignalSpy loginSpy(&service, &VpnService::loginStatusResult);

        service.checkLoginStatus();

        QCOMPARE(loginSpy.count(), 1);
        QCOMPARE(loginSpy.first().at(0).toBool(), true);
        QCOMPARE(loginSpy.first().at(1).toString(), QStringLiteral("user@example.com"));
        // info + status + config list all issued.
        QVERIFY(runner.invocations.size() >= 3);
    }

    void checkLoginStatus_signedOut_noRetries()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(okResult(QStringLiteral("Account: 'None'\n")));
        VpnService service(&runner);
        QSignalSpy loginSpy(&service, &VpnService::loginStatusResult);

        service.checkLoginStatus();

        QCOMPARE(loginSpy.count(), 1);
        QCOMPARE(loginSpy.first().at(0).toBool(), false);
        QCOMPARE(runner.invocations.size(), 1); // an explicit 'None' is final
    }

    void login_twoFactor_forwarded_andFinishStartsPolling()
    {
        FakeProcessRunner runner;
        VpnService service(&runner);
        QSignalSpy twoFaSpy(&service, &VpnService::twoFactorRequired);
        QSignalSpy finishedSpy(&service, &VpnService::loginFinished);

        service.login(QStringLiteral("alice"), QStringLiteral("hunter2"));
        QVERIFY(service.isLoginInProgress());

        runner.lastHandle->emitOutput(QStringLiteral("Password: "));
        runner.lastHandle->emitOutput(QStringLiteral("2FA Token: "));
        QCOMPARE(twoFaSpy.count(), 1);

        service.submit2fa(QStringLiteral("123456"));
        // Success path starts polling + account-type fetch.
        runner.cannedResults.append(okResult(QStringLiteral("Status: Disconnected\n")));
        runner.cannedResults.append(okResult(QString()));
        runner.lastHandle->emitFinished(0, QStringLiteral("Signed in.\n"));

        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.first().at(0).toBool(), true);
        QVERIFY(service.isLoginInProgress() == false);
    }

    void fetchCountries_emitsTypedList()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(okResult(
            QStringLiteral("Country  Code\n-------  ----\nSwitzerland  CH\nJapan  JP\n")));
        VpnService service(&runner);
        QSignalSpy spy(&service, &VpnService::countriesReady);

        service.fetchCountries();

        QCOMPARE(spy.count(), 1);
        const auto countries = spy.first().first().value<QList<Country>>();
        QCOMPARE(countries.size(), 2);
        QCOMPARE(countries.first().code, QStringLiteral("CH"));
    }

    void fetchCities_failure_doesNotEmit()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(failResult(QStringLiteral("not signed in")));
        VpnService service(&runner);
        QSignalSpy spy(&service, &VpnService::citiesReady);

        service.fetchCities(QStringLiteral("CH"));
        QCOMPARE(spy.count(), 0); // never clobber a good list with an empty one
    }

    void applyConfigValueAndReconnect_disconnectsSetsReconnects()
    {
        FakeProcessRunner runner;
        // connect first so last country/city are set
        runner.cannedResults.append(okResult(QStringLiteral("Connected.\n")));
        runner.cannedResults.append(okResult(QStringLiteral("Status: Connected\nServer: CH#7\n")));
        VpnService service(&runner);
        service.connectVpn(QStringLiteral("CH"), QStringLiteral("Zurich"));

        runner.cannedResults.append(okResult());                       // disconnect
        runner.cannedResults.append(okResult(QStringLiteral("set"))); // config set
        runner.cannedResults.append(okResult(QStringLiteral("Connected.\n"))); // reconnect
        runner.cannedResults.append(okResult(QStringLiteral("Status: Connected\nServer: CH#7\n")));

        QSignalSpy configSpy(&service, &VpnService::configApplied);
        service.applyConfigValueAndReconnect(QStringLiteral("kill-switch"),
                                             QStringLiteral("standard"));

        QCOMPARE(configSpy.count(), 1);
        QCOMPARE(service.state(), VpnState::Connected);

        // The reconnect used the remembered location.
        bool sawReconnect = false;
        for (const auto& inv : runner.invocations)
        {
            if (inv.args.contains(QStringLiteral("--country")) &&
                inv.args.contains(QStringLiteral("CH")) &&
                inv.args.contains(QStringLiteral("Zurich")))
            {
                sawReconnect = true;
            }
        }
        QVERIFY(sawReconnect);
    }

    void signOut_stopsPollingAndResets()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(okResult(QStringLiteral("Account: 'user@example.com'\n")));
        runner.cannedResults.append(okResult(QStringLiteral("Status: Disconnected\n")));
        runner.cannedResults.append(okResult(QString()));
        VpnService service(&runner);
        service.checkLoginStatus();

        runner.cannedResults.append(okResult());
        QSignalSpy spy(&service, &VpnService::signOutFinished);
        service.signOut();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toBool(), true);
        QCOMPARE(service.state(), VpnState::Disconnected);
    }
};

QTEST_MAIN(TstVpnService)
#include "tst_vpnService.moc"
