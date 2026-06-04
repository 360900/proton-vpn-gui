#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QSysInfo>
#include <QTranslator>
#include "appconfig.h"
#include "cli/flatpakutils.h"
#include "dbus/vpnstatusadaptor.h"
#include "debug.h"
#include "mainwindow.h"
#include "migrations.h"
#include "thememanager.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ProtonVPN"));
    QApplication::setApplicationDisplayName(QStringLiteral("ProtonVPN"));

    // Install translator for UI strings
    static QTranslator translator;
    const QLocale locale = QLocale::system();
    if (translator.load(locale, QStringLiteral("proton_vpn_qt"), QStringLiteral("_"), QStringLiteral(":/i18n")))
    {
        QApplication::installTranslator(&translator);
    }

    // Read version from the embedded version.json so there is a single source of truth.
    QString appVersion = QStringLiteral("unknown");
    QString cliVersionTestedMin = QStringLiteral("unknown");
    QString cliVersionTestedMax = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
        {
            appVersion = obj[QStringLiteral("app_version")].toString();
        }
        if (obj.contains(QStringLiteral("cli_version_tested_min")))
        {
            cliVersionTestedMin = obj[QStringLiteral("cli_version_tested_min")].toString();
        }
        if (obj.contains(QStringLiteral("cli_version_tested_max")))
        {
            cliVersionTestedMax = obj[QStringLiteral("cli_version_tested_max")].toString();
        }
    }
    QApplication::setApplicationVersion(appVersion);

    //  Startup diagnostics
    DBG_APP(QStringLiteral("=== ProtonVPN Qt App starting ==="));
    DBG_APP(QStringLiteral("App version        : ") + appVersion);
    DBG_APP(QStringLiteral("CLI tested min     : ") + cliVersionTestedMin);
    DBG_APP(QStringLiteral("CLI tested max     : ") + cliVersionTestedMax);
    DBG_APP(QStringLiteral("Qt version         : ") + QString::fromLatin1(qVersion()));
    DBG_APP(QStringLiteral("Package type       : ") + (isRunningAsFlatpak() ? QStringLiteral("Flatpak") : QStringLiteral("System")));
    DBG_APP(QStringLiteral("OS                 : ") + QSysInfo::prettyProductName());
    DBG_APP(QStringLiteral("Kernel             : ") + QSysInfo::kernelVersion());
    DBG_APP(QStringLiteral("CPU arch           : ") + QSysInfo::currentCpuArchitecture());
    DBG_APP(QStringLiteral("Locale             : ") + QLocale::system().name());
    DBG_APP(QStringLiteral("Config dir         : ") + QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    DBG_APP(QStringLiteral("================================="));

    // Single-instance guard - prevent multiple copies running at the same time.
    const QString lockPath = QDir::tempPath() + QStringLiteral("/proton-vpn-qt-app.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0); // never treat a lock as stale
    constexpr int lockTimeoutMs = 100;
    if (lockFile.tryLock(lockTimeoutMs) == false)
    {
        qint64 pid = 0;
        QString hostname;
        QString appname;
        lockFile.getLockInfo(&pid, &hostname, &appname);
        DBG_APP(QStringLiteral("Another instance of ProtonVPN is already running (PID: ") +
                (pid > 0 ? QString::number(pid) : QStringLiteral("unknown")) +
                QStringLiteral("). Exiting."));
        QMessageBox::warning(
            nullptr,
            QCoreApplication::translate("main", "Already Running"),
            QCoreApplication::translate("main", "ProtonVPN is already running.\n\nCheck your system tray or taskbar."));
        return 1;
    }

    // Use Breeze style on KDE Plasma if available, else Fusion
    const QStringList availableStyles = QStyleFactory::keys();
    if (availableStyles.contains(QStringLiteral("Breeze"), Qt::CaseInsensitive))
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Breeze")));
    }
    else if (availableStyles.contains(QStringLiteral("Fusion"), Qt::CaseInsensitive))
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    }

    // Apply theme (palette + stylesheet) based on saved preference.
    // This replaces the former hard-coded dark palette block.
    ThemeManager::apply(AppConfig::instance().theme());

    // Run one-time upgrade migrations before the main window is constructed.
    // lastSeenVersion() still holds the previous version at this point.
    Migrations::run(AppConfig::instance().lastSeenVersion());

    MainWindow w;

    // Expose VPN status on the D-Bus session bus
    // The adaptor must be parented to the adapted object (VpnManager)
    const VpnStatusAdaptor* dbusAdaptor = new VpnStatusAdaptor(w.manager());
    Q_UNUSED(dbusAdaptor)
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (sessionBus.isConnected())
    {
        sessionBus.registerObject(QStringLiteral("/com/protonvpn/app"), w.manager());
        if (sessionBus.registerService(QStringLiteral("com.protonvpn.app")) == false)
        {
            DBG_APP(QStringLiteral("D-Bus: failed to register service com.protonvpn.app: ") +
                    sessionBus.lastError().message());
        }
        else
        {
            DBG_APP(QStringLiteral("D-Bus: service com.protonvpn.app registered on session bus"));
        }
    }
    else
    {
        DBG_APP(QStringLiteral("D-Bus: cannot connect to session bus - VPN status will not be exposed"));
    }

    if (AppConfig::instance().startHidden())
    {
        w.hide();
    }
    else
    {
        w.show();
    }
    return QApplication::exec();
}

