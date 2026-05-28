#include <QtTest/QtTest>
#include <QProcessEnvironment>
#include "cli/flatpakutils.h"

// Tests for the inline helpers in flatpakutils.h:
//   isRunningAsFlatpak()   — env-var detection
//   buildHostCommand()     — transparent flatpak-spawn wrapping

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
            qputenv("FLATPAK_ID", "io.github.wheat32.ProtonVPNQt");
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
            qputenv("FLATPAK_ID", "io.github.wheat32.ProtonVPNQt");
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

    void buildHostCommand_inFlatpak_programIsFlatpakSpawn()
    {
        setFlatpakEnv(true);
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("status")});

        QCOMPARE(prog, QStringLiteral("flatpak-spawn"));
    }

    void buildHostCommand_inFlatpak_firstArgIsHost()
    {
        setFlatpakEnv(true);
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("status")});

        QVERIFY(!args.isEmpty());
        QCOMPARE(args.first(), QStringLiteral("--host"));
    }

    void buildHostCommand_inFlatpak_originalProgramIsSecondArg()
    {
        setFlatpakEnv(true);
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("connect")});

        QVERIFY(args.size() >= 2);
        QCOMPARE(args.at(1), QStringLiteral("protonvpn"));
    }

    void buildHostCommand_inFlatpak_originalArgsAppended()
    {
        setFlatpakEnv(true);
        const auto [prog, args] =
            buildHostCommand(QStringLiteral("protonvpn"),
                             {QStringLiteral("connect"), QStringLiteral("--cc"), QStringLiteral("DE")});

        // Expected: { "--host", "protonvpn", "connect", "--cc", "DE" }
        QCOMPARE(args.size(), 5);
        QCOMPARE(args.at(2), QStringLiteral("connect"));
        QCOMPARE(args.at(3), QStringLiteral("--cc"));
        QCOMPARE(args.at(4), QStringLiteral("DE"));
    }
};

QTEST_MAIN(TstFlatpakUtils)
#include "tst_flatpakutils.moc"

