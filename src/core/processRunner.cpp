// processRunner.cpp
// QProcessRunner - the single production implementation of ProcessRunner.

#include "processRunner.h"

#include "debug.h"
#include "hostCommand.h"

#include <QProcess>
#include <QTimer>
#include <unistd.h>

namespace
{
// Wraps a QProcess as an interactive ProcessHandle.
class QProcessHandle final : public ProcessHandle
{
public:
    QProcessHandle(const QString& program, const QStringList& args,
                   const bool detachFromTty, QObject* parent)
        : ProcessHandle(parent)
        , m_process(new QProcess(this))
    {
        if (detachFromTty)
        {
            // A child inheriting the controlling terminal makes Python's
            // getpass() read /dev/tty directly, bypassing our stdin pipe.
            // setsid() (between fork and exec) detaches the child from the
            // terminal so prompts go to the piped streams.
            m_process->setChildProcessModifier([]
            {
                ::setsid();
            });
        }

        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]
        {
            const QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());
            m_accumulated += chunk;
            emit outputReceived(chunk);
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this]
        {
            const QString chunk = QString::fromUtf8(m_process->readAllStandardError());
            m_accumulated += chunk;
            emit outputReceived(chunk);
        });
        connect(m_process, &QProcess::finished, this,
                [this](const int exitCode, QProcess::ExitStatus)
        {
            emit finished(exitCode, m_accumulated);
        });
        connect(m_process, &QProcess::errorOccurred, this,
                [this](const QProcess::ProcessError error)
        {
            if (error == QProcess::FailedToStart)
            {
                emit finished(-1, m_accumulated);
            }
        });

        m_process->start(program, args);
    }

    void writeStdin(const QByteArray& data) override
    {
        if (isRunning())
        {
            m_process->write(data);
        }
    }

    void kill() override
    {
        // Detach completion signals first: a deliberate kill must not be
        // reported as a finished run.
        disconnect(m_process, nullptr, this, nullptr);
        m_process->kill();
    }

    bool isRunning() const override
    {
        return m_process->state() == QProcess::Running;
    }

private:
    QProcess* m_process;
    QString   m_accumulated;
};
} // namespace

QProcessRunner::QProcessRunner(QObject* parent)
    : ProcessRunner(parent)
{
}

void QProcessRunner::run(const QString& program, const QStringList& args,
                         const int timeoutMs, Callback done)
{
    auto [prog, fullArgs] = buildHostCommand(program, args);

    QProcess* process = new QProcess(this);

    // Shared completion guard: finished, errorOccurred, and the timeout can
    // all fire; only the first outcome reaches the callback.
    auto finished = std::make_shared<bool>(false);

    QTimer* timeoutTimer = nullptr;
    if (timeoutMs > 0)
    {
        timeoutTimer = new QTimer(process);
        timeoutTimer->setSingleShot(true);
        timeoutTimer->setInterval(timeoutMs);
        connect(timeoutTimer, &QTimer::timeout, process,
                [process, done, finished, program]
                {
                    if (*finished)
                    {
                        return;
                    }
                    *finished = true;
                    DBG_CLI(QStringLiteral("Command timed out: ") + program);
                    Result result;
                    result.timedOut = true;
                    result.stdOut = QString::fromUtf8(process->readAllStandardOutput());
                    result.stdErr = QString::fromUtf8(process->readAllStandardError());
                    process->kill();
                    process->deleteLater();
                    done(result);
                });
        timeoutTimer->start();
    }

    connect(process, &QProcess::finished, this,
            [process, done, finished](const int exitCode, QProcess::ExitStatus)
            {
                if (*finished)
                {
                    return;
                }
                *finished = true;
                Result result;
                result.exitCode = exitCode;
                result.stdOut = QString::fromUtf8(process->readAllStandardOutput());
                result.stdErr = QString::fromUtf8(process->readAllStandardError());
                process->deleteLater();
                done(result);
            });

    connect(process, &QProcess::errorOccurred, this,
            [process, done, finished](const QProcess::ProcessError error)
            {
                if (error != QProcess::FailedToStart || *finished)
                {
                    return;
                }
                *finished = true;
                Result result;
                result.failedToStart = true;
                process->deleteLater();
                done(result);
            });

    process->start(prog, fullArgs);
}

ProcessHandle* QProcessRunner::startInteractive(const QString& program, const QStringList& args,
                                                const bool detachFromTty)
{
    auto [prog, fullArgs] = buildHostCommand(program, args);
    return new QProcessHandle(prog, fullArgs, detachFromTty, this);
}
