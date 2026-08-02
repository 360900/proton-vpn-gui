// vpnStateMachine.cpp
// See vpnStateMachine.h.

#include "vpnStateMachine.h"

#include "cliParsers.h"
#include "debug.h"

VpnStateMachine::VpnStateMachine(QObject* parent)
    : QObject(parent)
{
    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout, this, &VpnStateMachine::onWatchdogFired);
}

void VpnStateMachine::setWatchdogIntervals(const int firstIntervalMs, const int secondIntervalMs)
{
    m_firstIntervalMs  = firstIntervalMs;
    m_secondIntervalMs = secondIntervalMs;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void VpnStateMachine::beginConnecting()
{
    enterTransition(VpnState::Connecting);
}

void VpnStateMachine::beginDisconnecting()
{
    enterTransition(VpnState::Disconnecting);
}

void VpnStateMachine::connectSucceeded(const QString& message)
{
    leaveTransition(VpnState::Connected, message);
}

void VpnStateMachine::connectFailed(const QString& error)
{
    leaveTransition(VpnState::Error, error);
}

void VpnStateMachine::disconnectSucceeded(const QString& message)
{
    leaveTransition(VpnState::Disconnected, message);
}

void VpnStateMachine::disconnectFailed(const QString& error)
{
    leaveTransition(VpnState::Error, error);
}

void VpnStateMachine::reset(const VpnState state)
{
    const bool changed = m_state != state || m_connectedServer.isEmpty() == false;
    m_watchdog.stop();
    m_watchdogStage   = 0;
    m_state           = state;
    m_connectedServer.clear();
    m_disconnectedPollsWhileConnected = 0;
    // reset() travels outside the transition flow (e.g. sign-out), so announce
    // the change to D-Bus/tray/UI consumers just like any other state change.
    if (changed)
    {
        emit stateChanged(m_state, QString());
    }
}

// ---------------------------------------------------------------------------
// Snapshots
// ---------------------------------------------------------------------------

void VpnStateMachine::applySnapshot(const StatusSnapshot& snapshot)
{
    if (isTransitioning())
    {
        // A snapshot showing the transition's target state resolves it; one
        // still showing the source state means "in progress" and is ignored.
        const bool connectingDone = m_state == VpnState::Connecting &&
                                    snapshot.state == VpnState::Connected;
        const bool disconnectingDone = m_state == VpnState::Disconnecting &&
                                       snapshot.state == VpnState::Disconnected;
        if (connectingDone)
        {
            DBG_POLL(QStringLiteral("Poll resolved pending connect: %1").arg(snapshot.server));
            m_watchdog.stop();
            m_watchdogStage = 0;
            setConnected(snapshot.server,
                         snapshot.server.isEmpty()
                             ? QString()
                             : QStringLiteral("Connected to %1.").arg(snapshot.server));
        }
        else if (disconnectingDone)
        {
            DBG_POLL(QStringLiteral("Poll resolved pending disconnect."));
            leaveTransition(VpnState::Disconnected, QString());
        }
        return;
    }

    if (snapshot.state == VpnState::Connected)
    {
        m_disconnectedPollsWhileConnected = 0;
    }

    const bool stateChangedNow = snapshot.state != m_state;
    const bool serverChangedNow = stateChangedNow == false &&
                                  snapshot.state == VpnState::Connected &&
                                  m_connectedServer.isEmpty() == false &&
                                  snapshot.server != m_connectedServer;

    if (stateChangedNow)
    {
        DBG_POLL(QStringLiteral("State changed:  %1 -> %2")
                     .arg(CliParsers::vpnStateToString(m_state),
                          CliParsers::vpnStateToString(snapshot.state)));
    }
    else if (serverChangedNow)
    {
        DBG_POLL(QStringLiteral("Server changed: \"%1\" -> \"%2\"")
                     .arg(m_connectedServer, snapshot.server));
    }

    if (stateChangedNow || serverChangedNow)
    {
        if (snapshot.state == VpnState::Connected)
        {
            m_disconnectedPollsWhileConnected = 0;
            setConnected(snapshot.server,
                         snapshot.server.isEmpty()
                             ? QString()
                             : QStringLiteral("Connected to %1.").arg(snapshot.server));
        }
        else
        {
            if (m_state == VpnState::Connected && m_disconnectedPollsWhileConnected == 0)
            {
                // A status request can briefly return the pre-connect state
                // while the daemon finishes updating its session. Do not
                // flash the UI to Disconnected on that first contradiction.
                ++m_disconnectedPollsWhileConnected;
                DBG_POLL(QStringLiteral("Ignoring one stale disconnected poll while connected."));
                return;
            }
            m_disconnectedPollsWhileConnected = 0;
            m_state = VpnState::Disconnected;
            m_connectedServer.clear();
            emit stateChanged(m_state, QString());
        }
    }
    else if (snapshot.state == VpnState::Connected && m_connectedServer.isEmpty() &&
             snapshot.server.isEmpty() == false)
    {
        // Same state, but we finally learned which server we are on (first
        // poll after a connect command, or app started while already
        // connected). No state change to announce, but the location is new
        // information the UI uses to pre-select pickers.
        m_connectedServer = snapshot.server;
        const ServerInfo parsed = CliParsers::parseServerInfo(snapshot.server);
        if (parsed.city.isEmpty() == false)
        {
            emit connectionCityKnown(parsed.city);
        }
        if (parsed.countryCode.isEmpty() == false)
        {
            emit connectionCountryKnown(parsed.countryCode);
        }
    }
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

void VpnStateMachine::enterTransition(const VpnState transitionState)
{
    m_state = transitionState;
    m_watchdogStage = 1;
    m_watchdog.start(m_firstIntervalMs);
    emit stateChanged(m_state, QString());
}

void VpnStateMachine::leaveTransition(const VpnState newState, const QString& info)
{
    m_watchdog.stop();
    m_watchdogStage = 0;
    m_state = newState;
    if (newState != VpnState::Connected)
    {
        m_connectedServer.clear();
    }
    emit stateChanged(m_state, info);
}

void VpnStateMachine::setConnected(const QString& server, const QString& info)
{
    m_state           = VpnState::Connected;
    m_connectedServer = server;
    m_disconnectedPollsWhileConnected = 0;

    const ServerInfo parsed = CliParsers::parseServerInfo(server);
    if (parsed.city.isEmpty() == false)
    {
        emit connectionCityKnown(parsed.city);
    }
    if (parsed.countryCode.isEmpty() == false)
    {
        emit connectionCountryKnown(parsed.countryCode);
    }
    emit stateChanged(m_state, info);
}

void VpnStateMachine::onWatchdogFired()
{
    if (isTransitioning() == false)
    {
        m_watchdogStage = 0;
        return;
    }

    if (m_watchdogStage == 1)
    {
        // First stage: the transition is taking suspiciously long - ask for
        // an immediate status poll, which may resolve it either way.
        DBG_POLL(QStringLiteral("Transition watchdog: still %1 after %2 ms - forcing poll.")
                     .arg(CliParsers::vpnStateToString(m_state))
                     .arg(m_firstIntervalMs));
        m_watchdogStage = 2;
        m_watchdog.start(m_secondIntervalMs);
        emit forcePollRequested();
        return;
    }

    // Second stage: still transitioning - give up and surface an error the
    // user can act on instead of a spinner that never stops.
    DBG_POLL(QStringLiteral("Transition watchdog: %1 never resolved - degrading to Error.")
                 .arg(CliParsers::vpnStateToString(m_state)));
    m_watchdogStage = 0;
    const QString info = m_state == VpnState::Connecting
        ? QStringLiteral("Connecting timed out. The VPN daemon did not respond - "
                         "try disconnecting and connecting again.")
        : QStringLiteral("Disconnecting timed out. The VPN daemon did not respond.");
    m_state = VpnState::Error;
    m_connectedServer.clear();
    emit transitionTimedOut();
    emit stateChanged(m_state, info);
}
