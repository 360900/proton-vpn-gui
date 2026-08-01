#pragma once
// fakeProcessRunner.h
// Test double for ProcessRunner: records every invocation and replays canned
// results synchronously, so CLI-client and service tests run without spawning
// a single process.

#include "core/processRunner.h"

#include <QList>

class FakeProcessHandle final : public ProcessHandle
{
public:
    using ProcessHandle::ProcessHandle;

    void writeStdin(const QByteArray& data) override { writtenStdin += data; }
    void kill() override
    {
        running = false;
        killed  = true;
    }
    bool isRunning() const override { return running; }

    // Test drivers.
    void emitOutput(const QString& chunk) { emit outputReceived(chunk); }
    void emitFinished(const int exitCode, const QString& combined)
    {
        running = false;
        emit finished(exitCode, combined);
    }

    QByteArray writtenStdin;
    bool       running = true;
    bool       killed  = false;
};

class FakeProcessRunner final : public ProcessRunner
{
public:
    using ProcessRunner::ProcessRunner;

    struct Invocation
    {
        QString     program;
        QStringList args;
        int         timeoutMs = 0;
        bool        interactive = false;
        bool        detachFromTty = false;
    };

    void run(const QString& program, const QStringList& args,
             const int timeoutMs, Callback done) override
    {
        invocations.append({program, args, timeoutMs, false, false});
        if (deferred)
        {
            pendingCallbacks.append(std::move(done));
            return;
        }
        Result result;
        result.exitCode = 0;
        if (cannedResults.isEmpty() == false)
        {
            result = cannedResults.takeFirst();
        }
        done(result);
    }

    // Deferred mode: run() parks its callback until the test flushes it,
    // simulating a slow process (used to test in-flight guards).
    void flushNext(const Result& result)
    {
        Callback cb = pendingCallbacks.takeFirst();
        cb(result);
    }

    ProcessHandle* startInteractive(const QString& program, const QStringList& args,
                                    const bool detachFromTty = false) override
    {
        invocations.append({program, args, -1, true, detachFromTty});
        lastHandle = new FakeProcessHandle(this);
        return lastHandle;
    }

    QList<Invocation> invocations;
    QList<Result>     cannedResults; // consumed front-first; default: exit 0, empty output
    bool              deferred = false;
    QList<Callback>   pendingCallbacks;
    FakeProcessHandle* lastHandle = nullptr;
};
