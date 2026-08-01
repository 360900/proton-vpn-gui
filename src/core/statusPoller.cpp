// statusPoller.cpp
// See statusPoller.h.

#include "statusPoller.h"

#include "debug.h"

#include <QPointer>

StatusPoller::StatusPoller(ProtonVpnCliClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &StatusPoller::tick);
}

void StatusPoller::setIntervals(const int steadyIntervalMs, const int transitionIntervalMs)
{
    m_steadyIntervalMs     = steadyIntervalMs;
    m_transitionIntervalMs = transitionIntervalMs;
}

void StatusPoller::start()
{
    if (m_stopped == false)
    {
        return;
    }
    m_stopped = false;
    DBG_STATUS(QStringLiteral("Status poller started (steady %1 ms, transition %2 ms).")
                   .arg(m_steadyIntervalMs).arg(m_transitionIntervalMs));
    tick(); // poll immediately, then keep rescheduling
}

void StatusPoller::stop()
{
    m_stopped = true;
    m_timer.stop();
    DBG_STATUS(QStringLiteral("Status poller stopped."));
}

void StatusPoller::setTransitionMode(const bool inTransition)
{
    if (m_inTransition == inTransition)
    {
        return;
    }
    m_inTransition = inTransition;
    if (m_stopped)
    {
        return;
    }
    // Re-arm at the new cadence; entering transition mode polls right away
    // so the fast cadence starts immediately.
    if (inTransition)
    {
        pollNow();
    }
    else if (m_inflight == false)
    {
        m_timer.start(currentInterval());
    }
}

void StatusPoller::pollNow()
{
    // Deliberately works even while stopped: a one-shot poll (e.g. right
    // after a connect command resolves) is always safe - only the periodic
    // rescheduling below is gated on the started state.
    m_timer.stop();
    tick();
}

void StatusPoller::tick()
{
    if (m_inflight)
    {
        // A previous poll is still running - skip, the completion handler
        // will reschedule.
        return;
    }
    m_inflight = true;

    QPointer<StatusPoller> self(this);
    m_client->status([self](const bool ok, const StatusSnapshot& snapshot)
    {
        if (self.isNull())
        {
            return;
        }
        self->m_inflight = false;
        if (ok)
        {
            emit self->snapshotReady(snapshot);
        }
        if (self->m_stopped == false)
        {
            self->m_timer.start(self->currentInterval());
        }
    });
}
