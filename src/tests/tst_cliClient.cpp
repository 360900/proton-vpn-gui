#include <QtTest/QtTest>

#include "core/cliClient.h"
#include "support/fakeProcessRunner.h"

// ProtonVpnCliClient tests, driven entirely through FakeProcessRunner -
// no process is ever spawned. Verifies both directions of the contract:
// the exact argv the client asks for, and how raw results are turned into
// typed callbacks.

class TstCliClient : public QObject
{
    Q_OBJECT

private slots:

    void status_requestsStatusVerb_andParsesSnapshot()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 0;
        canned.stdOut = QStringLiteral("Status: Connected\nServer: DE#42 in Frankfurt, Germany\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        bool called = false;
        client.status([&](const bool ok, const StatusSnapshot& s)
        {
            called = true;
            QVERIFY(ok);
            QCOMPARE(s.state, VpnState::Connected);
            QCOMPARE(s.server, QStringLiteral("DE#42 in Frankfurt, Germany"));
        });

        QVERIFY(called);
        QCOMPARE(runner.invocations.size(), 1);
        QCOMPARE(runner.invocations.first().program, QStringLiteral("protonvpn"));
        QCOMPARE(runner.invocations.first().args, QStringList{QStringLiteral("status")});
    }

    void connectTo_buildsCountryAndCityArgs()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);

        client.connectTo(QStringLiteral("CH"), QStringLiteral("Zurich"),
                         [](bool, const QString&, const QString&) {});

        const QStringList expected{
            QStringLiteral("connect"),
            QStringLiteral("--country"), QStringLiteral("CH"),
            QStringLiteral("--city"),    QStringLiteral("Zurich")};
        QCOMPARE(runner.invocations.first().args, expected);
    }

    void connectTo_fastest_hasNoLocationArgs()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);
        client.connectTo(QString(), QString(), [](bool, const QString&, const QString&) {});
        QCOMPARE(runner.invocations.first().args, QStringList{QStringLiteral("connect")});
    }

    void connectTo_success_stripsNoiseFromMessage()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 0;
        canned.stdOut = QStringLiteral("Connected to CH#7.\n"
                                       "To get your forwarded port, run natpmpc periodically:\n"
                                       "Guide: https://protonvpn.com/support/port-forwarding\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.connectTo(QString(), QString(),
                         [](const bool ok, const QString& message, const QString&)
        {
            QVERIFY(ok);
            QCOMPARE(message.trimmed(), QStringLiteral("Connected to CH#7."));
        });
    }

    void connectTo_failure_reportsStderr()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 1;
        canned.stdErr = QStringLiteral("No servers available\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.connectTo(QString(), QString(),
                         [](const bool ok, const QString&, const QString& error)
        {
            QVERIFY(ok == false);
            QCOMPARE(error, QStringLiteral("No servers available"));
        });
    }

    void countries_parsesStdoutOnly()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 0;
        canned.stdOut = QStringLiteral("Country  Code\n-------  ----\nSwitzerland  CH\n");
        // Python-runtime warnings on stderr must not become rows.
        canned.stdErr = QStringLiteral("framework.  For more detail see\n"
                                       "  from eventlet.patcher import x  # type: ignore\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.countries([](const bool ok, const QList<Country>& countries)
        {
            QVERIFY(ok);
            QCOMPARE(countries.size(), 1);
            QCOMPARE(countries.first(), (Country{QStringLiteral("Switzerland"), QStringLiteral("CH")}));
        });
    }

    void checkInstalled_flatpakSpawnFailure_isNotInstalled()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 1;
        canned.stdErr = QStringLiteral("Portal call failed: Failed to start command\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.checkInstalled([](const bool installed) { QVERIFY(installed == false); });
    }

    void checkInstalled_helpOutput_isInstalled()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 0;
        canned.stdErr = QStringLiteral("Usage: protonvpn [OPTIONS] COMMAND\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.checkInstalled([](const bool installed) { QVERIFY(installed); });
    }

    void accountName_signedIn()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 0;
        canned.stdOut = QStringLiteral("Account: 'user@example.com'\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.accountName([](const bool responded, const QString& account)
        {
            QVERIFY(responded);
            QCOMPARE(account, QStringLiteral("user@example.com"));
        });
    }

    void accountName_signedOut_isEmptyButResponded()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 0;
        canned.stdOut = QStringLiteral("Account: 'None'\n");
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.accountName([](const bool responded, const QString& account)
        {
            QVERIFY(responded);
            QVERIFY(account.isEmpty());
        });
    }

    void accountName_noAnswer_notResponded()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result canned;
        canned.exitCode = 1;
        runner.cannedResults.append(canned);

        ProtonVpnCliClient client(&runner);
        client.accountName([](const bool responded, const QString&)
        {
            QVERIFY(responded == false);
        });
    }

    //  LoginSession

    void login_passwordFedOnPrompt()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);

        LoginSession* session =
            client.signin(QStringLiteral("alice"), QStringLiteral("hunter2"));
        QVERIFY(session != nullptr);
        QCOMPARE(runner.invocations.first().args,
                 (QStringList{QStringLiteral("signin"), QStringLiteral("alice")}));
        QVERIFY(runner.invocations.first().detachFromTty);

        runner.lastHandle->emitOutput(QStringLiteral("Password: "));
        QCOMPARE(runner.lastHandle->writtenStdin, QByteArrayLiteral("hunter2\n"));
    }

    void login_twoFactorPrompt_emitsSignal_andTokenIsForwarded()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);

        LoginSession* session =
            client.signin(QStringLiteral("alice"), QStringLiteral("hunter2"));
        QSignalSpy twoFaSpy(session, &LoginSession::twoFactorRequired);

        runner.lastHandle->emitOutput(QStringLiteral("Password: "));
        runner.lastHandle->emitOutput(QStringLiteral("\n2FA Token: "));
        QCOMPARE(twoFaSpy.count(), 1);

        session->submit2fa(QStringLiteral("123456"));
        QVERIFY(runner.lastHandle->writtenStdin.endsWith(QByteArrayLiteral("123456\n")));
    }

    void login_finished_reportsParsedResult()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);

        LoginSession* session =
            client.signin(QStringLiteral("alice"), QStringLiteral("hunter2"));
        QSignalSpy finishedSpy(session, &LoginSession::finished);

        runner.lastHandle->emitFinished(0, QStringLiteral("Password:\nSigned in.\n"));
        QCOMPARE(finishedSpy.count(), 1);
        const auto result = finishedSpy.first().first().value<LoginResult>();
        QVERIFY(result.ok);
    }

    void login_cancel_killsProcess_andSuppressesFinished()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);

        LoginSession* session =
            client.signin(QStringLiteral("alice"), QStringLiteral("hunter2"));
        QSignalSpy finishedSpy(session, &LoginSession::finished);

        session->cancel();
        QVERIFY(runner.lastHandle->killed);
        QCOMPARE(finishedSpy.count(), 0);
    }
};

QTEST_MAIN(TstCliClient)
#include "tst_cliClient.moc"
