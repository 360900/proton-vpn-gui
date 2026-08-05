#include <QtTest/QtTest>
#include <QProcessEnvironment>
#include "core/hostCommand.h"

// Tests for the inline helpers in core/hostCommand.h:
//   isRunningAsFlatpak()   - env-var detection
//   buildHostCommand()     - returns the bundled CLI (in-sandbox) unchanged

class TstFlatpakUtils : public QObject
{
    Q_OBJECT

private:
    // Save/restore FLATPAK_ID so tests don't pollute each other.
    bool m_hadFlatpakId = false;

    void setFlatpakEnv(bool set)
    {
        if (set)
        {
            qputenv("FLATPAK_ID", "io.github._360900.Vela");
        }
        else
        {
            qunsetenv("FLATPAK_ID");
        }
    }

private slots:

    void init()
    {
        // Record whether the test was already launched inside a Flatpak.
        m_hadFlatpakId = qEnvironmentVariableIsSet("FLATPAK_ID");
        qunsetenv("FLATPAK_ID");
    }

    void cleanup()
    {
        if (m_hadFlatpakId)
            qputenv("FLATPAK_ID", "io.github._360900.Vela");
        else
            qunsetenv("FLATPAK_ID");
    }

    //  isRunningAsFlatpak

    void isRunningAsFlatpak_envNotSet_returnsFalse()
    {
        QVERIFY(!isRunningAsFlatpak());
    }

    void isRunningAsFlatpak_envSet_returnsTrue()
    {
        setFlatpakEnv(true);
        QVERIFY(isRunningAsFlatpak());
    }

    //  buildHostCommand

    void buildHostCommand_notFlatpak_returnsProgramUnchanged()
    {
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("connect"), QStringLiteral("--cc"), QStringLiteral("US")});

        QCOMPARE(prog, QStringLiteral("protonvpn"));
        QCOMPARE(args, (QStringList{QStringLiteral("connect"),
                                    QStringLiteral("--cc"),
                                    QStringLiteral("US")}));
    }

    void buildHostCommand_notFlatpak_noArgs_returnsEmptyArgList()
    {
        const auto [prog, args] = buildHostCommand(QStringLiteral("protonvpn"));
        QCOMPARE(prog, QStringLiteral("protonvpn"));
        QVERIFY(args.isEmpty());
    }

    void buildHostCommand_inFlatpak_returnsBundledProgramUnchanged()
    {
        setFlatpakEnv(true);
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("status")});

        // The CLI is bundled in the sandbox: no flatpak-spawn --host wrapping.
        QCOMPARE(prog, QStringLiteral("protonvpn"));
        QCOMPARE(args, (QStringList{QStringLiteral("status")}));
    }

    void buildHostCommand_inFlatpak_originalArgsAppended()
    {
        setFlatpakEnv(true);
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("connect"), QStringLiteral("--cc"), QStringLiteral("DE")});

        QCOMPARE(prog, QStringLiteral("protonvpn"));
        QCOMPARE(args, (QStringList{QStringLiteral("connect"),
                                    QStringLiteral("--cc"),
                                    QStringLiteral("DE")}));
    }
};

QTEST_MAIN(TstFlatpakUtils)
#include "tst_flatpakUtils.moc"
