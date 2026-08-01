#include <QtTest/QtTest>

#include "core/processRunner.h"

// Integration tests for QProcessRunner using tiny real processes (echo,
// sleep, /bin/sh). Runs outside any sandbox, so buildHostCommand() is a
// pass-through; the flatpak-spawn wrapping itself is covered by
// tst_flatpakUtils.

class TstProcessRunner : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        // Make buildHostCommand() deterministic even if the test environment
        // leaks a FLATPAK_ID.
        qunsetenv("FLATPAK_ID");
    }

    void run_capturesStdout()
    {
        QProcessRunner runner;
        ProcessRunner::Result got;
        bool done = false;
        runner.run(QStringLiteral("echo"), {QStringLiteral("hello")}, 5000,
                   [&](const ProcessRunner::Result& r)
                   {
                       got  = r;
                       done = true;
                   });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(got.ok());
        QCOMPARE(got.stdOut.trimmed(), QStringLiteral("hello"));
    }

    void run_capturesStderrAndExitCode()
    {
        QProcessRunner runner;
        ProcessRunner::Result got;
        bool done = false;
        runner.run(QStringLiteral("/bin/sh"),
                   {QStringLiteral("-c"), QStringLiteral("echo oops >&2; exit 3")}, 5000,
                   [&](const ProcessRunner::Result& r)
                   {
                       got  = r;
                       done = true;
                   });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QCOMPARE(got.exitCode, 3);
        QCOMPARE(got.stdErr.trimmed(), QStringLiteral("oops"));
        QVERIFY(got.ok() == false);
    }

    void run_missingBinary_failsToStart()
    {
        QProcessRunner runner;
        ProcessRunner::Result got;
        bool done = false;
        runner.run(QStringLiteral("definitely-not-a-real-binary-xyz"), {}, 5000,
                   [&](const ProcessRunner::Result& r)
                   {
                       got  = r;
                       done = true;
                   });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(got.failedToStart);
        QVERIFY(got.ok() == false);
    }

    void run_timeout_killsAndReports()
    {
        QProcessRunner runner;
        ProcessRunner::Result got;
        bool done = false;
        runner.run(QStringLiteral("sleep"), {QStringLiteral("30")}, 200,
                   [&](const ProcessRunner::Result& r)
                   {
                       got  = r;
                       done = true;
                   });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(got.timedOut);
        QVERIFY(got.ok() == false);
    }

    void run_callbackInvokedExactlyOnce()
    {
        QProcessRunner runner;
        int calls = 0;
        runner.run(QStringLiteral("true"), {}, 5000,
                   [&](const ProcessRunner::Result&) { ++calls; });
        QTRY_VERIFY_WITH_TIMEOUT(calls > 0, 5000);
        // Give any stray duplicate signal a chance to fire.
        QTest::qWait(100);
        QCOMPARE(calls, 1);
    }

    void interactive_echoRoundTrip()
    {
        QProcessRunner runner;
        // `cat` echoes stdin back to stdout - a perfect interactive stub.
        ProcessHandle* handle =
            runner.startInteractive(QStringLiteral("cat"), {});

        QString output;
        int  exitCode = -1;
        bool finished = false;
        connect(handle, &ProcessHandle::outputReceived,
                [&](const QString& chunk) { output += chunk; });
        connect(handle, &ProcessHandle::finished,
                [&](const int code, const QString&)
                {
                    exitCode = code;
                    finished = true;
                });

        QTRY_VERIFY_WITH_TIMEOUT(handle->isRunning(), 5000);
        handle->writeStdin(QByteArrayLiteral("ping\n"));
        QTRY_VERIFY_WITH_TIMEOUT(output.contains(QStringLiteral("ping")), 5000);

        handle->writeStdin(QByteArray()); // no-op
        // Closing stdin is not exposed; kill() ends the session as cancel would.
        handle->kill();
        QTest::qWait(100);
        QVERIFY(finished == false); // kill suppresses the finished signal
        Q_UNUSED(exitCode)
    }
};

QTEST_MAIN(TstProcessRunner)
#include "tst_processRunner.moc"
