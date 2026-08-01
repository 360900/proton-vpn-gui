#include <QtTest/QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "appConfig.h"

// AppConfig is a singleton, so all test slots share the same instance.
// We use QStandardPaths::setTestModeEnabled(true) to redirect file I/O to a
// throwaway location so we never touch the real ~/.config/ProtonVPN-GUI/.

class TstAppConfig : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Redirect all standard paths to a temporary location so the tests
        // never read from (or write to) the user's real config directory.
        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanupTestCase()
    {
        QStandardPaths::setTestModeEnabled(false);
    }

    //  Default values
    // With no pre-existing config file the singleton must expose the defaults
    // documented in appConfig.h.
    void defaults_autoConnectIsFalse()
    {
        QCOMPARE(AppConfig::instance().autoConnect(), false);
    }

    void defaults_notificationsIsTrue()
    {
        QCOMPARE(AppConfig::instance().notifications(), true);
    }

    void defaults_recentConnectionsCountIsFive()
    {
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 5);
    }

    void defaults_startHiddenIsFalse()
    {
        QCOMPARE(AppConfig::instance().startHidden(), false);
    }

    void defaults_themeIsSystem()
    {
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::System);
    }

    //  Setters round-trip
    void setAutoConnect_storesValue()
    {
        AppConfig::instance().setAutoConnect(true);
        QCOMPARE(AppConfig::instance().autoConnect(), true);
        // Restore
        AppConfig::instance().setAutoConnect(false);
    }

    void setNotifications_storesValue()
    {
        AppConfig::instance().setNotifications(false);
        QCOMPARE(AppConfig::instance().notifications(), false);
        // Restore
        AppConfig::instance().setNotifications(true);
    }

    void setStartHidden_storesValue()
    {
        AppConfig::instance().setStartHidden(true);
        QCOMPARE(AppConfig::instance().startHidden(), true);
        // Restore
        AppConfig::instance().setStartHidden(false);
    }

    //  Theme round-trip
    void setTheme_dark_storesValue()
    {
        AppConfig::instance().setTheme(AppConfig::Theme::Dark);
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::Dark);
    }

    void setTheme_light_storesValue()
    {
        AppConfig::instance().setTheme(AppConfig::Theme::Light);
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::Light);
    }

    void setTheme_system_storesValue()
    {
        AppConfig::instance().setTheme(AppConfig::Theme::System);
        QCOMPARE(AppConfig::instance().theme(), AppConfig::Theme::System);
    }

    //  recentConnectionsCount boundary
    void setRecentConnectionsCount_negativeValue_clampsToZero()
    {
        AppConfig::instance().setRecentConnectionsCount(-1);
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 0);
        // Restore to default
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    void setRecentConnectionsCount_zero_allowed()
    {
        AppConfig::instance().setRecentConnectionsCount(0);
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 0);
        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }

    void setRecentConnectionsCount_positiveValue_storesValue()
    {
        AppConfig::instance().setRecentConnectionsCount(10);
        QCOMPARE(AppConfig::instance().recentConnectionsCount(), 10);
        // Restore
        AppConfig::instance().setRecentConnectionsCount(5);
    }
};

QTEST_MAIN(TstAppConfig)
#include "tst_appConfig.moc"

