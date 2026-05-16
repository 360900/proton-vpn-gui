// statusmonitor.cpp
// StatusMonitor: long-lived subprocess that runs `protonvpn status` in a
// 15-second loop.  See statusmonitor.h for the full description.

#include "statusmonitor.h"
#include "flatpakutils.h"

#include <QDebug>
#include <QTimer>
#include <ranges>
#include <signal.h>
#include <sys/prctl.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// The process will appear under this name in ps/top/htop/bpftrace/journalctl.
// Linux TASK_COMM_LEN is 16 bytes (15 usable chars); the full name is always
// visible in /proc/PID/cmdline and `ps aux`.
static constexpr char kProcessName[] = "protonvpn-qt-status-mon";

// Shell command run by the subprocess:
//   1. exec -a renames the bash process to kProcessName (sets argv[0]).
//   2. The inner bash runs an infinite loop:
//        a. `protonvpn status` (stdout + stderr merged) — or via flatpak-spawn
//           when running inside a Flatpak sandbox.
//        b. ASCII 0x1E (Record Separator) — unambiguous snapshot delimiter
//        c. sleep 15
static QString buildLoopCommand()
{
    // Inside a Flatpak sandbox, `protonvpn` is not available directly — it
    // must be forwarded to the host via flatpak-spawn.
    const QString vpnCmd = isRunningAsFlatpak()
        ? QStringLiteral("flatpak-spawn --host protonvpn status")
        : QStringLiteral("protonvpn status");

    return QStringLiteral("exec -a protonvpn-qt-status-mon /bin/bash -c "
                          "'while true; "
                          "do %1 2>&1; "
                          "printf \"\\x1e\"; "
                          "sleep 15; "
                          "done'").arg(vpnCmd);
}

// How long to wait before restarting the monitor after an unexpected exit.
static constexpr int kRestartDelayMs = 5'000;

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

StatusMonitor::StatusMonitor(QObject* parent)
    : QObject(parent)
{
}

StatusMonitor::~StatusMonitor()
{
    stop();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void StatusMonitor::start()
{
    if (m_process)
    {
#ifdef QT_DEBUG
        qDebug("[StatusMonitor] start() called but monitor is already running "
               "(PID %lld) — ignored.",
               m_process->processId());
#endif
        return;
    }

    m_stopping = false;
    launchProcess();
}

void StatusMonitor::stop()
{
    m_stopping = true;

    if (!m_process)
        return;

#ifdef QT_DEBUG
    qDebug("[StatusMonitor] Stopping (PID %lld).",
           static_cast<long long>(m_process->processId()));
#endif

    // Disconnect finished() so onProcessFinished() does not schedule an
    // auto-restart after we deliberately kill the process.
    disconnect(m_process, nullptr, this, nullptr);
    m_process->kill();
    m_process->deleteLater();
    m_process = nullptr;
    m_buffer.clear();
}

bool StatusMonitor::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

// ---------------------------------------------------------------------------
// Private — process lifecycle
// ---------------------------------------------------------------------------

void StatusMonitor::launchProcess()
{
    if (m_stopping)
        return;

    m_buffer.clear();
    m_process = new QProcess(this);

    // Ask the kernel to deliver SIGTERM to this child if the parent process
    // dies for any reason, including SIGKILL.  The lambda runs in the child
    // after fork() but before exec(), which is the only safe place to call
    // prctl().  This prevents protonvpn-qt-status-mon from becoming an orphan
    // if the GUI process is hard-killed or crashes.
    m_process->setChildProcessModifier([]
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);
    });

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &StatusMonitor::onReadyRead);

    connect(m_process,
            &QProcess::finished,
            this, &StatusMonitor::onProcessFinished);

    const QString loopCommand = buildLoopCommand();
    m_process->start(QStringLiteral("/bin/bash"),
                     {QStringLiteral("-c"), loopCommand});

#ifdef QT_DEBUG
    // waitForStarted so we can log the PID immediately.
    if (m_process->waitForStarted(2000))
    {
        qDebug("[StatusMonitor] Process launched:"
               "  name=\"%s\"  PID=%lld  restart#=%d"
               "  command: /bin/bash -c \"%s\"",
               kProcessName,
               m_process->processId(),
               m_restartCount,
               qUtf8Printable(loopCommand));
    }
    else
    {
        qDebug("[StatusMonitor] ERROR: process failed to start within 2 s "
               "(QProcess::ProcessError=%d).",
               m_process->error());
    }
#endif
}

void StatusMonitor::onReadyRead()
{
    const QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());

#ifdef QT_DEBUG
    qDebug("[StatusMonitor] Raw data received (%lld bytes).",
           chunk.size());
#endif

    m_buffer += chunk;

    // Each complete `protonvpn status` snapshot is terminated by ASCII 0x1E
    // (Record Separator).  Accumulate until we see the delimiter.
    int sepPos;
    while ((sepPos = m_buffer.indexOf(QLatin1Char('\x1e'))) != -1)
    {
        const QString snapshot = m_buffer.left(sepPos);
        m_buffer.remove(0, sepPos + 1);

        const QMap<QString, QString> fields = parseStatusFields(snapshot);

#ifdef QT_DEBUG
        qDebug("[StatusMonitor] Snapshot parsed:"
               "  status=\"%s\"  server=\"%s\"  fields=%lld.",
               qUtf8Printable(fields.value(QStringLiteral("status"))),
               qUtf8Printable(fields.value(QStringLiteral("server"))),
               static_cast<long long>(fields.size()));
#endif

        emit statusParsed(fields);
    }
}

void StatusMonitor::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
#ifdef QT_DEBUG
    // CrashExit means the process was terminated by a signal — either an
    // external `kill`/`kill -9` or the kernel via PR_SET_PDEATHSIG.
    // NormalExit with a non-zero code means the shell loop exited on its own.
    if (status == QProcess::CrashExit)
        qDebug("[StatusMonitor] Process was killed externally (signal termination)."
               "  restart#=%d — restarting in %d ms.",
               m_restartCount,
               kRestartDelayMs);
    else
        qDebug("[StatusMonitor] Process exited unexpectedly (non-zero exit):"
               "  exitCode=%d  restart#=%d — restarting in %d ms.",
               exitCode,
               m_restartCount,
               kRestartDelayMs);
#else
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
#endif

    m_process->deleteLater();
    m_process = nullptr;
    m_buffer.clear();
    ++m_restartCount;

    QTimer::singleShot(kRestartDelayMs, this, &StatusMonitor::launchProcess);
}

// ---------------------------------------------------------------------------
// Static parsing helpers
// ---------------------------------------------------------------------------

// static
QMap<QString, QString> StatusMonitor::parseStatusFields(const QString& combined)
{
    QStringList lines = combined.split(QLatin1Char('\n'));

    // Remove noise / informational lines that are not "Key: Value" data.
    lines.erase(std::ranges::remove_if(lines, [](const QString& l)
    {
        const QString ll = l.toLower();
        return ll.contains(QLatin1String("outdated"))                          ||
               ll.contains(QLatin1String("updating"))                          ||
               ll.contains(QLatin1String("this may take"))                     ||
               ll.contains(QLatin1String("to get your forwarded port"))        ||
               ll.contains(QLatin1String("natpmpc"))                           ||
               (ll.startsWith(QLatin1String("guide:")) &&
                ll.contains(QLatin1String("http")));
    }).begin(), lines.end());

    QMap<QString, QString> fields;
    for (const QString& line : std::as_const(lines))
    {
        const int colonPos = line.indexOf(QLatin1Char(':'));
        if (colonPos < 0) continue;
        const QString key   = line.left(colonPos).trimmed().toLower();
        const QString value = line.mid(colonPos + 1).trimmed();
        if (!key.isEmpty() && !value.isEmpty())
            fields.insert(key, value);
    }
    return fields;
}

// static
QString StatusMonitor::parseCityFromServer(const QString& server)
{
    const int inPos = server.indexOf(QStringLiteral(" in "));
    if (inPos < 0)
        return {};
    const QString rest    = server.mid(inPos + 4);
    const int    commaPos = rest.indexOf(QLatin1Char(','));
    return (commaPos >= 0 ? rest.left(commaPos) : rest).trimmed();
}
