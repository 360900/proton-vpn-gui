#include <QtTest/QtTest>

#include "core/vpnStateMachine.h"

// VpnStateMachine tests. The watchdog runs with millisecond intervals here;
// production uses 30 s + 30 s.

namespace
{
StatusSnapshot connectedSnapshot(const QString& server)
{
    StatusSnapshot s;
    s.state  = VpnState::Connected;
    s.server = server;
    return s;
}

StatusSnapshot disconnectedSnapshot()
{
    StatusSnapshot s;
    s.state = VpnState::Disconnected;
    return s;
}
} // namespace

class TstVpnStateMachine : public QObject
{
    Q_OBJECT

private slots:

    //  Command-driven transitions

    void connectFlow_success()
    {
        VpnStateMachine sm;
        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);

        sm.beginConnecting();
        QCOMPARE(sm.state(), VpnState::Connecting);

        sm.connectSucceeded(QStringLiteral("Connected to CH#7."));
        QCOMPARE(sm.state(), VpnState::Connected);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.last().at(1).toString(), QStringLiteral("Connected to CH#7."));
    }

    void connectFlow_failure_setsError()
    {
        VpnStateMachine sm;
        sm.beginConnecting();
        sm.connectFailed(QStringLiteral("No servers available"));
        QCOMPARE(sm.state(), VpnState::Error);
    }

    void disconnectFlow_success_clearsServer()
    {
        VpnStateMachine sm;
        sm.beginConnecting();
        sm.applySnapshot(connectedSnapshot(QStringLiteral("CH#7 in Zurich, Switzerland")));
        QCOMPARE(sm.connectedServer(), QStringLiteral("CH#7 in Zurich, Switzerland"));

        sm.beginDisconnecting();
        sm.disconnectSucceeded(QString());
        QCOMPARE(sm.state(), VpnState::Disconnected);
        QVERIFY(sm.connectedServer().isEmpty());
    }

    //  Snapshot application during transitions

    void snapshotDuringConnecting_targetState_resolvesTransition()
    {
        VpnStateMachine sm;
        QSignalSpy citySpy(&sm, &VpnStateMachine::connectionCityKnown);
        QSignalSpy countrySpy(&sm, &VpnStateMachine::connectionCountryKnown);

        sm.beginConnecting();
        sm.applySnapshot(connectedSnapshot(QStringLiteral("US-NJ#203 in Secaucus, United States")));

        QCOMPARE(sm.state(), VpnState::Connected);
        QCOMPARE(sm.connectedServer(), QStringLiteral("US-NJ#203 in Secaucus, United States"));
        QCOMPARE(citySpy.count(), 1);
        QCOMPARE(citySpy.first().first().toString(), QStringLiteral("Secaucus"));
        QCOMPARE(countrySpy.first().first().toString(), QStringLiteral("US"));
    }

    void snapshotDuringConnecting_sourceState_isIgnored()
    {
        VpnStateMachine sm;
        sm.beginConnecting();
        sm.applySnapshot(disconnectedSnapshot()); // tunnel not up yet - still in progress
        QCOMPARE(sm.state(), VpnState::Connecting);
    }

    void snapshotDuringDisconnecting_targetState_resolvesTransition()
    {
        VpnStateMachine sm;
        sm.beginConnecting();
        sm.applySnapshot(connectedSnapshot(QStringLiteral("CH#7")));
        sm.beginDisconnecting();
        sm.applySnapshot(connectedSnapshot(QStringLiteral("CH#7"))); // still up - ignored
        QCOMPARE(sm.state(), VpnState::Disconnecting);
        sm.applySnapshot(disconnectedSnapshot());
        QCOMPARE(sm.state(), VpnState::Disconnected);
    }

    //  Steady-state snapshots

    void steadySnapshot_externalConnect_isDetected()
    {
        VpnStateMachine sm;
        sm.reset(VpnState::Disconnected);
        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);

        sm.applySnapshot(connectedSnapshot(QStringLiteral("DE#42 in Frankfurt, Germany")));
        QCOMPARE(sm.state(), VpnState::Connected);
        QCOMPARE(spy.count(), 1);
    }

    void steadySnapshot_sameState_noSignal()
    {
        VpnStateMachine sm;
        sm.reset(VpnState::Disconnected);
        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);
        sm.applySnapshot(disconnectedSnapshot());
        QCOMPARE(spy.count(), 0);
    }

    void steadySnapshot_serverChange_emits()
    {
        VpnStateMachine sm;
        sm.reset(VpnState::Disconnected);
        sm.applySnapshot(connectedSnapshot(QStringLiteral("DE#42 in Frankfurt, Germany")));
        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);

        sm.applySnapshot(connectedSnapshot(QStringLiteral("DE#43 in Berlin, Germany")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(sm.connectedServer(), QStringLiteral("DE#43 in Berlin, Germany"));
    }

    void steadySnapshot_oneDisconnectedPollAfterConnect_isIgnored()
    {
        VpnStateMachine sm;
        sm.reset(VpnState::Disconnected);
        sm.applySnapshot(connectedSnapshot(QStringLiteral("DE#42 in Frankfurt, Germany")));

        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);
        sm.applySnapshot(disconnectedSnapshot());

        QCOMPARE(sm.state(), VpnState::Connected);
        QCOMPARE(spy.count(), 0);

        sm.applySnapshot(disconnectedSnapshot());

        QCOMPARE(sm.state(), VpnState::Disconnected);
        QCOMPARE(spy.count(), 1);
    }

    void steadySnapshot_lateServerName_filledInSilently()
    {
        // App started while already connected: first snapshot had no server
        // recorded yet; learning it must not re-emit a state change.
        VpnStateMachine sm;
        sm.reset(VpnState::Connected);
        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);
        sm.applySnapshot(connectedSnapshot(QStringLiteral("CH#7")));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(sm.connectedServer(), QStringLiteral("CH#7"));
    }

    //  reset (sign-out path)

    void reset_fromConnected_emitsStateChanged()
    {
        // Sign-out happens outside the transition flow; consumers (tray,
        // D-Bus adaptor, UI) must still learn about it, otherwise they keep
        // showing the stale "Connected" state.
        VpnStateMachine sm;
        sm.reset(VpnState::Disconnected);
        sm.applySnapshot(connectedSnapshot(QStringLiteral("CH#7")));

        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);
        sm.reset(VpnState::Disconnected);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).value<VpnState>(), VpnState::Disconnected);
    }

    void reset_sameState_noEmit()
    {
        VpnStateMachine sm;
        sm.applySnapshot(disconnectedSnapshot());

        QSignalSpy spy(&sm, &VpnStateMachine::stateChanged);
        sm.reset(VpnState::Disconnected);
        QCOMPARE(spy.count(), 0);
    }

    //  Watchdog

    void watchdog_firstStage_requestsPoll()
    {
        VpnStateMachine sm;
        sm.setWatchdogIntervals(20, 1000);
        QSignalSpy pollSpy(&sm, &VpnStateMachine::forcePollRequested);

        sm.beginConnecting();
        QTRY_COMPARE_WITH_TIMEOUT(pollSpy.count(), 1, 2000);
        QCOMPARE(sm.state(), VpnState::Connecting); // not yet degraded
    }

    void watchdog_secondStage_degradesToError()
    {
        VpnStateMachine sm;
        sm.setWatchdogIntervals(20, 20);
        QSignalSpy timeoutSpy(&sm, &VpnStateMachine::transitionTimedOut);
        QSignalSpy stateSpy(&sm, &VpnStateMachine::stateChanged);

        sm.beginConnecting();
        QTRY_COMPARE_WITH_TIMEOUT(timeoutSpy.count(), 1, 2000);
        QCOMPARE(sm.state(), VpnState::Error);
        QVERIFY(stateSpy.last().at(1).toString().contains(QStringLiteral("timed out")));
    }

    void watchdog_resolvedTransition_neverFires()
    {
        VpnStateMachine sm;
        sm.setWatchdogIntervals(50, 50);
        QSignalSpy pollSpy(&sm, &VpnStateMachine::forcePollRequested);
        QSignalSpy timeoutSpy(&sm, &VpnStateMachine::transitionTimedOut);

        sm.beginConnecting();
        sm.connectSucceeded(QString());
        QTest::qWait(200);
        QCOMPARE(pollSpy.count(), 0);
        QCOMPARE(timeoutSpy.count(), 0);
        QCOMPARE(sm.state(), VpnState::Connected);
    }
};

QTEST_MAIN(TstVpnStateMachine)
#include "tst_vpnStateMachine.moc"
