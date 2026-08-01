#pragma once
// statusPoller.h
// QTimer-driven `protonvpn status` polling. Replaces the old StatusMonitor's
// long-lived `bash -c 'while true; ... sleep 15'` subprocess with one-shot
// CLI runs through ProtonVpnCliClient (and therefore ProcessRunner, which
// handles flatpak-spawn forwarding).
//
// Cadence: steadyIntervalMs (default 15 s) normally, transitionIntervalMs
// (default 2 s) while a connect/disconnect is in flight - fast resolution of
// transitions is what fixes the old stuck-Connecting bug. An in-flight guard
// ensures polls never overlap.

#include "cliClient.h"

#include <QObject>
#include <QTimer>

class StatusPoller final : public QObject
{
    Q_OBJECT

public:
    explicit StatusPoller(ProtonVpnCliClient* client, QObject* parent = nullptr);

    void setIntervals(int steadyIntervalMs, int transitionIntervalMs);

    void start();
    void stop();
    bool isActive() const { return m_timer.isActive(); }

    // Switch between steady and transition cadence.
    void setTransitionMode(bool inTransition);

    // Run a poll immediately (watchdog nudge / right after a connect result).
    void pollNow();

signals:
    void snapshotReady(const StatusSnapshot& snapshot);

private:
    void tick();
    int currentInterval() const
    {
        return m_inTransition ? m_transitionIntervalMs : m_steadyIntervalMs;
    }

    ProtonVpnCliClient* m_client;
    QTimer m_timer;
    bool   m_inTransition         = false;
    bool   m_inflight             = false;
    bool   m_stopped              = true;
    int    m_steadyIntervalMs     = 15'000;
    int    m_transitionIntervalMs = 2'000;
};
