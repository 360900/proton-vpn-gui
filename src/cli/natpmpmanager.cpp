// natpmpmanager.cpp
// NatPmpManager: NAT-PMP port-forwarding keep-alive loop.
// All UI concerns live in the consumer (VpnPage); this class only manages
// the process lifecycle and emits result signals.

#include "natpmpmanager.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

NatPmpManager::NatPmpManager(QObject* parent)
    : QObject(parent)
{
}

bool NatPmpManager::isInstalled()
{
    return !QStandardPaths::findExecutable(QStringLiteral("natpmpc")).isEmpty();
}

void NatPmpManager::start()
{
    if (m_timer)
        return; // already running

    if (QStandardPaths::findExecutable(QStringLiteral("natpmpc")).isEmpty())
    {
        emit natpmpcMissing();
        return;
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(45'000); // 45 s keeps a 60 s NAT-PMP lease alive
    connect(m_timer, &QTimer::timeout, this, &NatPmpManager::run);
    m_timer->start();

    run(); // fire immediately so the port appears without a 45 s wait
}

void NatPmpManager::refresh()
{
    if (!m_timer)
    {
        start(); // not yet running — start() calls run() immediately
        return;
    }
    // Loop already running: fire an immediate request if none is in flight.
    if (!m_active)
        run();
}

void NatPmpManager::stop()
{
    if (m_timer)
    {
        m_timer->stop();
        m_timer->deleteLater();
        m_timer = nullptr;
    }
    m_active        = false;
    m_forwardedPort = 0;
}

void NatPmpManager::run()
{
    if (m_active)
        return; // previous invocation still in flight

    m_active = true;

    auto* process = new QProcess(this);
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](const int exitCode, QProcess::ExitStatus)
    {
        m_active = false;
        const QString out = QString::fromUtf8(process->readAllStandardOutput())
                          + QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();

        if (exitCode != 0)
        {
            if (!isInstalled())
            {
                // natpmpc was uninstalled during runtime — clear the displayed
                // port first, then stop the loop and signal the missing binary.
                if (m_forwardedPort != 0)
                {
                    m_forwardedPort = 0;
                    emit portLost();
                }
                stop();
                emit natpmpcMissing();
            }
            else if (m_forwardedPort != 0)
            {
                m_forwardedPort = 0;
                emit portLost();
            }
            return;
        }

        // Example output: "Mapped public port 53186 protocol UDP lifetime 60"
        const QRegularExpression re(QStringLiteral(R"(Mapped public port (\d+))"));
        const QRegularExpressionMatch match = re.match(out);
        if (match.hasMatch())
        {
            m_forwardedPort = match.captured(1).toInt();
            emit portAcquired(m_forwardedPort);
        }
    });

    // Both UDP and TCP mappings are required by the ProtonVPN port-forwarding
    // spec; the allocated port number is the same for both and is parsed from
    // the UDP response.
    process->start(QStringLiteral("/bin/sh"),
                   {QStringLiteral("-c"),
                    QStringLiteral("natpmpc -a 1 0 udp 60 -g 10.2.0.1 "
                                   "&& natpmpc -a 1 0 tcp 60 -g 10.2.0.1")});
}

