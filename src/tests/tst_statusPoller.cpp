#include <QtTest/QtTest>

#include "core/statusPoller.h"
#include "support/fakeProcessRunner.h"

// StatusPoller tests through a real ProtonVpnCliClient over FakeProcessRunner.

namespace
{
ProcessRunner::Result connectedResult()
{
    ProcessRunner::Result r;
    r.exitCode = 0;
    r.stdOut = QStringLiteral("Status: Connected\nServer: CH#7 in Zurich, Switzerland\n");
    return r;
}
} // namespace

class TstStatusPoller : public QObject
{
    Q_OBJECT

private slots:

    void start_pollsImmediately_andEmitsSnapshot()
    {
        FakeProcessRunner runner;
        runner.cannedResults.append(connectedResult());
        ProtonVpnCliClient client(&runner);
        StatusPoller poller(&client);
        poller.setIntervals(60'000, 60'000); // no reschedule during test

        QSignalSpy spy(&poller, &StatusPoller::snapshotReady);
        poller.start();

        QCOMPARE(spy.count(), 1);
        const auto snapshot = spy.first().first().value<StatusSnapshot>();
        QCOMPARE(snapshot.state, VpnState::Connected);
        QCOMPARE(runner.invocations.size(), 1);
        QCOMPARE(runner.invocations.first().args, QStringList{QStringLiteral("status")});
    }

    void steadyCadence_repolls()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);
        StatusPoller poller(&client);
        poller.setIntervals(20, 20);

        poller.start();
        QTRY_VERIFY_WITH_TIMEOUT(runner.invocations.size() >= 3, 2000);
    }

    void failedPoll_doesNotEmit_butKeepsPolling()
    {
        FakeProcessRunner runner;
        ProcessRunner::Result bad;
        bad.exitCode = 1;
        runner.cannedResults.append(bad);
        ProtonVpnCliClient client(&runner);
        StatusPoller poller(&client);
        poller.setIntervals(20, 20);

        QSignalSpy spy(&poller, &StatusPoller::snapshotReady);
        poller.start();
        QCOMPARE(spy.count(), 0);                                    // failure not surfaced
        QTRY_VERIFY_WITH_TIMEOUT(runner.invocations.size() >= 2, 2000); // but polling continues
    }

    void inflightGuard_neverOverlaps()
    {
        FakeProcessRunner runner;
        runner.deferred = true;
        ProtonVpnCliClient client(&runner);
        StatusPoller poller(&client);
        poller.setIntervals(10, 10);

        poller.start();
        QCOMPARE(runner.invocations.size(), 1);

        // The first poll is still "running" (deferred); pollNow must not
        // start a second process.
        poller.pollNow();
        QCOMPARE(runner.invocations.size(), 1);

        // Completing the first poll frees the guard and re-arms the timer.
        runner.flushNext(connectedResult());
        QTRY_VERIFY_WITH_TIMEOUT(runner.invocations.size() >= 2, 2000);
        runner.flushNext(connectedResult());
    }

    void stop_haltsPolling()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);
        StatusPoller poller(&client);
        poller.setIntervals(10, 10);

        poller.start();
        poller.stop();
        const int countAtStop = runner.invocations.size();
        QTest::qWait(100);
        QCOMPARE(runner.invocations.size(), countAtStop);
    }

    void transitionMode_pollsImmediately()
    {
        FakeProcessRunner runner;
        ProtonVpnCliClient client(&runner);
        StatusPoller poller(&client);
        poller.setIntervals(60'000, 60'000);

        poller.start();
        QCOMPARE(runner.invocations.size(), 1);

        poller.setTransitionMode(true); // entering a transition polls right away
        QCOMPARE(runner.invocations.size(), 2);
    }
};

QTEST_MAIN(TstStatusPoller)
#include "tst_statusPoller.moc"
