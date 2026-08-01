#pragma once
// vpnStateMachine.h
// Owns the VpnState and the connected-server string, applies status
// snapshots, and watches over transitions.
//
// The old implementation dropped status polls entirely while Connecting or
// Disconnecting, so a hung `protonvpn connect` left the UI stuck forever.
// This machine instead:
//   - resolves a transition as soon as a snapshot shows the target state
//     (snapshots showing the source state are ignored - still in progress),
//   - runs a watchdog while transitional: after firstIntervalMs it asks for
//     an immediate poll (forcePollRequested), after secondIntervalMs it gives
//     up and degrades to Error (transitionTimedOut + stateChanged).

#include "cliTypes.h"

#include <QObject>
#include <QString>
#include <QTimer>

class VpnStateMachine final : public QObject
{
    Q_OBJECT

public:
    explicit VpnStateMachine(QObject* parent = nullptr);

    VpnState state() const { return m_state; }
    QString  connectedServer() const { return m_connectedServer; }

    // Watchdog intervals; injectable so tests can run in milliseconds.
    void setWatchdogIntervals(int firstIntervalMs, int secondIntervalMs);

    // Commands (user-driven transitions) ----------------------------------

    void beginConnecting();
    void beginDisconnecting();

    // Results of the corresponding CLI commands.
    void connectSucceeded(const QString& message);
    void connectFailed(const QString& error);
    void disconnectSucceeded(const QString& message);
    void disconnectFailed(const QString& error);

    // Force a state outside the normal flow (e.g. sign-out).
    void reset(VpnState state = VpnState::Disconnected);

    // Status snapshots (poller-driven) ------------------------------------

    void applySnapshot(const StatusSnapshot& snapshot);

signals:
    void stateChanged(VpnState state, const QString& info);

    // Emitted (before stateChanged) when a connected server string is parsed,
    // so the UI can pre-select country/city pickers.
    void connectionCityKnown(const QString& city);
    void connectionCountryKnown(const QString& countryCode);

    // Watchdog: please run a status poll right now.
    void forcePollRequested();

    // Watchdog: the transition never resolved; state has been set to Error.
    void transitionTimedOut();

private:
    bool isTransitioning() const
    {
        return m_state == VpnState::Connecting || m_state == VpnState::Disconnecting;
    }

    void enterTransition(VpnState transitionState);
    void leaveTransition(VpnState newState, const QString& info);
    void setConnected(const QString& server, const QString& info);
    void onWatchdogFired();

    VpnState m_state = VpnState::Unknown;
    QString  m_connectedServer;

    QTimer m_watchdog;
    int    m_watchdogStage      = 0;      // 0 idle, 1 first interval armed, 2 second armed
    int    m_firstIntervalMs    = 30'000;
    int    m_secondIntervalMs   = 30'000; // after the first, i.e. 60 s total
    int    m_disconnectedPollsWhileConnected = 0;
};
