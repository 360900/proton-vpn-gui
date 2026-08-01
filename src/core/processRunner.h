#pragma once
// processRunner.h
// Injectable process-spawning abstraction. QProcessRunner is the ONLY place
// in the application allowed to call buildHostCommand()/QProcess::start for
// host commands - this structurally enforces the rule that every spawn is
// forwarded through flatpak-spawn when sandboxed.
//
// Tests inject a FakeProcessRunner (tests/support/fakeProcessRunner.h) to
// exercise the CLI client and services without spawning anything.

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// A long-lived interactive child process (e.g. `protonvpn signin`, which
// prompts for a password / 2FA token on its output streams).
class ProcessHandle : public QObject
{
    Q_OBJECT

public:
    explicit ProcessHandle(QObject* parent = nullptr) : QObject(parent) {}

    virtual void writeStdin(const QByteArray& data) = 0;
    virtual void kill() = 0;
    virtual bool isRunning() const = 0;

signals:
    // Emitted for every chunk of output; stdout and stderr are merged because
    // interactive prompts may arrive on either stream.
    void outputReceived(const QString& chunk);

    // Emitted exactly once when the process ends. combinedOutput is the full
    // accumulated stdout+stderr.
    void finished(int exitCode, const QString& combinedOutput);
};

class ProcessRunner : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        int     exitCode      = -1;
        QString stdOut;
        QString stdErr;
        bool    timedOut      = false;
        bool    failedToStart = false;

        bool ok() const { return exitCode == 0 && timedOut == false && failedToStart == false; }
    };
    using Callback = std::function<void(const Result&)>;

    explicit ProcessRunner(QObject* parent = nullptr) : QObject(parent) {}

    // Run a one-shot host command; done is invoked exactly once on the
    // caller's thread. timeoutMs <= 0 disables the timeout.
    virtual void run(const QString& program, const QStringList& args,
                     int timeoutMs, Callback done) = 0;

    // Start a long-lived interactive host command. The returned handle is
    // parented to this runner. detachFromTty runs the child in a new session
    // (setsid) so terminal-aware prompts (Python getpass) fall back to the
    // piped streams instead of /dev/tty.
    virtual ProcessHandle* startInteractive(const QString& program, const QStringList& args,
                                            bool detachFromTty = false) = 0;
};

// Production implementation: QProcess + buildHostCommand().
class QProcessRunner final : public ProcessRunner
{
    Q_OBJECT

public:
    explicit QProcessRunner(QObject* parent = nullptr);

    void run(const QString& program, const QStringList& args,
             int timeoutMs, Callback done) override;

    ProcessHandle* startInteractive(const QString& program, const QStringList& args,
                                    bool detachFromTty = false) override;
};
