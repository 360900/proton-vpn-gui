#pragma once
// natPmpService.h
// NAT-PMP port-forwarding keep-alive loop (`natpmpc`), replacing the old
// NatPmpManager. Differences:
//   - every spawn goes through ProcessRunner, so it works inside Flatpak
//     (the old code called natpmpc directly and silently failed sandboxed),
//   - the gateway is configurable (Proton's default 10.2.0.1),
//   - the install check runs on the HOST (`command -v natpmpc`), not against
//     the sandbox PATH.

#include "processRunner.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>

class NatPmpService final : public QObject
{
    Q_OBJECT

public:
    explicit NatPmpService(ProcessRunner* runner, QObject* parent = nullptr);

    // Proton's VPN-side NAT-PMP gateway; configurable in case the topology
    // ever changes.
    void setGateway(const QString& gateway);
    QString gateway() const { return m_gateway; }

    // Asynchronously checks whether natpmpc exists on the host.
    void checkInstalled(const std::function<void(bool installed)>& done) const;

    // Start the keep-alive loop (immediate first request, then every 45 s).
    void start();

    // Fire an immediate request; starts the loop if it is not running.
    void refresh();

    // Stop the loop and reset state.
    void stop();

    bool isRunning()     const { return m_timer.isActive(); }
    int  forwardedPort() const { return m_forwardedPort; }

    // Extract the mapped port from natpmpc output
    // ("Mapped public port 53186 protocol UDP lifetime 60"); 0 if absent.
    static int parseMappedPort(const QString& output);

signals:
    // Emitted each time the port lease is successfully renewed.
    void portAcquired(int port);

    // Emitted when natpmpc fails - the port is no longer valid.
    void portLost();

    // Emitted when natpmpc turns out not to be installed on the host.
    void natpmpcMissing();

private:
    void runOnce();

    ProcessRunner* m_runner;
    QString m_gateway = QStringLiteral("10.2.0.1");
    QTimer  m_timer;
    bool    m_active        = false; // a request is in flight
    int     m_forwardedPort = 0;
};
