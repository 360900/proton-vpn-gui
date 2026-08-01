#include <QtTest/QtTest>

#include "core/natPmpService.h"
#include "support/fakeProcessRunner.h"

// NatPmpService tests via FakeProcessRunner - verifies the exact shell
// command (including the configurable gateway), port parsing, and the
// lost/missing flows. Nothing is spawned.

namespace
{
ProcessRunner::Result mappedResult(const int port)
{
    ProcessRunner::Result r;
    r.exitCode = 0;
    r.stdOut = QStringLiteral("Mapped public port %1 protocol UDP lifetime 60\n"
                              "Mapped public port %1 protocol TCP lifetime 60\n").arg(port);
    return r;
}
} // namespace

class TstNatPmpService : public QObject
{
    Q_OBJECT

private slots:

    void parseMappedPort_extractsPort()
    {
        QCOMPARE(NatPmpService::parseMappedPort(
                     QStringLiteral("Mapped public port 53186 protocol UDP lifetime 60")),
                 53186);
        QCOMPARE(NatPmpService::parseMappedPort(QStringLiteral("no mapping here")), 0);
    }

    void start_runsKeepalive_withDefaultGateway()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(mappedResult(53186));
        NatPmpService service(&runner);

        QSignalSpy portSpy(&service, &NatPmpService::portAcquired);
        service.start();

        QCOMPARE(runner.invocations.size(), 1);
        QCOMPARE(runner.invocations.first().program, QStringLiteral("/bin/sh"));
        const QString cmd = runner.invocations.first().args.last();
        QVERIFY(cmd.contains(QStringLiteral("natpmpc -a 1 0 udp 60 -g 10.2.0.1")));
        QVERIFY(cmd.contains(QStringLiteral("natpmpc -a 1 0 tcp 60 -g 10.2.0.1")));

        QCOMPARE(portSpy.count(), 1);
        QCOMPARE(portSpy.first().first().toInt(), 53186);
        QCOMPARE(service.forwardedPort(), 53186);
        QVERIFY(service.isRunning());
    }

    void gatewayOverride_isUsedInCommand()
    {
        FakeProcessRunner runner;
        NatPmpService service(&runner);
        service.setGateway(QStringLiteral("10.9.0.1"));
        service.start();
        QVERIFY(runner.invocations.first().args.last()
                    .contains(QStringLiteral("-g 10.9.0.1")));
    }

    void failureWithBinaryPresent_emitsPortLost()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(mappedResult(53186));
        NatPmpService service(&runner);
        service.start();
        QCOMPARE(service.forwardedPort(), 53186);

        QSignalSpy lostSpy(&service, &NatPmpService::portLost);
        // Next tick fails; the follow-up `command -v natpmpc` check succeeds.
        ProcessRunner::Result fail;
        fail.exitCode = 1;
        runner.cannedResults.append(fail);            // keep-alive failure
        ProcessRunner::Result present;
        present.exitCode = 0;
        present.stdOut = QStringLiteral("/usr/bin/natpmpc\n");
        runner.cannedResults.append(present);         // command -v result
        service.refresh();

        QCOMPARE(lostSpy.count(), 1);
        QCOMPARE(service.forwardedPort(), 0);
        QVERIFY(service.isRunning()); // loop keeps trying while binary exists
    }

    void failureWithBinaryMissing_stopsAndEmitsMissing()
    {
        FakeProcessRunner runner;
        NatPmpService service(&runner);

        QSignalSpy missingSpy(&service, &NatPmpService::natpmpcMissing);
        ProcessRunner::Result fail;
        fail.exitCode = 127;
        runner.cannedResults.append(fail);            // keep-alive failure
        ProcessRunner::Result absent;
        absent.exitCode = 1;
        runner.cannedResults.append(absent);          // command -v: not found
        service.start();

        QCOMPARE(missingSpy.count(), 1);
        QVERIFY(service.isRunning() == false);
    }

    void checkInstalled_probesHostPath()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result present;
        present.exitCode = 0;
        runner.cannedResults.append(present);
        NatPmpService service(&runner);

        bool result = false;
        service.checkInstalled([&](const bool installed) { result = installed; });
        QVERIFY(result);
        QCOMPARE(runner.invocations.first().args.last(),
                 QStringLiteral("command -v natpmpc"));
    }

    void stop_resetsPort()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(mappedResult(4242));
        NatPmpService service(&runner);
        service.start();
        service.stop();
        QCOMPARE(service.forwardedPort(), 0);
        QVERIFY(service.isRunning() == false);
    }
};

QTEST_MAIN(TstNatPmpService)
#include "tst_natPmpService.moc"
