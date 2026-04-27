#pragma once

#include <QObject>

class QTimer;

// ---------------------------------------------------------------------------
// NatPmpManager
//
// Manages the natpmpc keep-alive loop required by ProtonVPN port forwarding.
// Runs `natpmpc -a 1 0 udp 60 -g 10.2.0.1 && natpmpc -a 1 0 tcp 60 -g 10.2.0.1`
// every 45 seconds to maintain the 60-second NAT-PMP lease.
//
// Consumers connect to the signals and call start()/stop() as the VPN state
// changes.  All UI concerns are left to the consumer.
// ---------------------------------------------------------------------------
class NatPmpManager : public QObject
{
    Q_OBJECT

public:
    explicit NatPmpManager(QObject* parent = nullptr);

    // Start the keep-alive loop.
    // Emits natpmpcMissing() immediately and returns without starting if the
    // natpmpc binary is not found on PATH.
    void start();

    // Stop the loop and reset internal state.
    void stop();

    bool isRunning()     const { return m_timer != nullptr; }
    int  forwardedPort() const { return m_forwardedPort; }

signals:
    // Emitted each time the port lease is successfully renewed.
    void portAcquired(int port);

    // Emitted when natpmpc exits non-zero — the port is no longer valid.
    void portLost();

    // Emitted from start() when natpmpc is not installed.
    void natpmpcMissing();

private:
    void run(); // single natpmpc invocation

    QTimer* m_timer         = nullptr;
    bool    m_active        = false;  // true while a process is in flight
    int     m_forwardedPort = 0;
};

