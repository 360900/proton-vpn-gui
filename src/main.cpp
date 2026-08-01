#include <QApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
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
#include "appConfig.h"
#include "cli/appImageUtils.h"
#include "cli/flatpakUtils.h"
#include "cli/platformUtils.h"
#include "dbus/vpnControlAdaptor.h"
#include "dbus/vpnStatusAdaptor.h"
#include "debug.h"
#include "fileLogger.h"
#include "mainWindow.h"
#include "migrations.h"
#include "themeManager.h"

int main(int argc, char* argv[])
{
    const QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Proton VPN GUI"));
    QApplication::setApplicationDisplayName(QStringLiteral("Proton VPN GUI"));

    // Install translator for UI strings
    static QTranslator translator;
    const QLocale locale = QLocale::system();
    if (translator.load(locale, QStringLiteral("proton_vpn_gui"), QStringLiteral("_"), QStringLiteral(":/i18n")))
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

    // Mirror DBG_* output to a rotating log file if the user has enabled it.
    FileLogger::instance().setEnabled(AppConfig::instance().logToFile());

    //  Startup diagnostics
    DBG_APP(QStringLiteral("=== Proton VPN GUI starting ==="));
    DBG_APP(QStringLiteral("App version        : ") + appVersion);
    DBG_APP(QStringLiteral("CLI tested min     : ") + cliVersionTestedMin);
    DBG_APP(QStringLiteral("CLI tested max     : ") + cliVersionTestedMax);
    DBG_APP(QStringLiteral("Qt version         : ") + QString::fromLatin1(qVersion()));
    DBG_APP(QStringLiteral("Package type       : ") + packageTypeName());
    DBG_APP(QStringLiteral("OS                 : ") + QSysInfo::prettyProductName());
    DBG_APP(QStringLiteral("Kernel             : ") + QSysInfo::kernelVersion());
    DBG_APP(QStringLiteral("CPU arch           : ") + QSysInfo::currentCpuArchitecture());
    DBG_APP(QStringLiteral("Locale             : ") + QLocale::system().name());
    DBG_APP(QStringLiteral("Config dir         : ") + QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    DBG_APP(QStringLiteral("================================="));

    AppConfig::instance().logLoadedConfig();

    // Single-instance guard. Primary mechanism: D-Bus service-name ownership -
    // owning io.github._360900.ProtonVpnGui means we are the only instance, and
    // a second launch asks the first to raise its window instead of showing a
    // warning box. Fallback when no session bus exists: a QLockFile.
    const QString dbusServiceName = QStringLiteral("io.github._360900.ProtonVpnGui");
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    QLockFile lockFile(QDir::tempPath() + QStringLiteral("/proton-vpn-gui.lock"));
    if (sessionBus.isConnected())
    {
        if (sessionBus.registerService(dbusServiceName) == false)
        {
            DBG_APP(QStringLiteral("Another instance owns %1 - raising it and exiting.")
                        .arg(dbusServiceName));
            QDBusMessage raise = QDBusMessage::createMethodCall(
                dbusServiceName,
                QStringLiteral("/io/github/360900/ProtonVpnGui"),
                QStringLiteral("io.github._360900.ProtonVpnGui.Control"),
                QStringLiteral("Raise"));
            sessionBus.call(raise, QDBus::Block, 2000);
            return 0;
        }
        DBG_APP(QStringLiteral("D-Bus: service %1 registered on session bus").arg(dbusServiceName));
    }
    else
    {
        DBG_APP(QStringLiteral("D-Bus: no session bus - falling back to lock-file guard"));
        lockFile.setStaleLockTime(0); // never treat a lock as stale
        constexpr int lockTimeoutMs = 100;
        if (lockFile.tryLock(lockTimeoutMs) == false)
        {
            QMessageBox::warning(
                nullptr,
                QCoreApplication::translate("main", "Already Running"),
                QCoreApplication::translate("main", "Proton VPN GUI is already running.\n\nCheck your system tray or taskbar."));
            return 1;
        }
    }

    // The Standalone AppImage bundles its own Proton VPN CLI. If a separate
    // CLI is also installed on the host, the two installs fight over the
    // same daemon/session state. Refuse to start rather than risk that.
    if (isStandaloneAppImage() && systemProtonVpnCliInstalledSeparately())
    {
        DBG_APP(QStringLiteral("Standalone AppImage detected a separate system Proton VPN CLI install. Refusing to start."));
        QMessageBox::critical(
            nullptr,
            QCoreApplication::translate("main", "Conflicting Proton VPN CLI Installation"),
            QCoreApplication::translate("main",
                "This AppImage bundles its own Proton VPN CLI, but a separate Proton VPN CLI "
                "installation was also found on your system. Running both together can cause "
                "conflicts.\n\nPlease either use the Lite AppImage instead, or uninstall the "
                "system Proton VPN CLI."));
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

    // Expose VPN status + control on the session bus. Adaptors must be
    // parented to the adapted object (VpnManager); the service name itself
    // was registered by the single-instance guard above. The name matches
    // the application ID so it is automatically owned inside a Flatpak
    // sandbox (no extra finish-args required).
    if (sessionBus.isConnected())
    {
        VpnService* service = w.manager()->service();
        const VpnStatusAdaptor* statusAdaptor = new VpnStatusAdaptor(service);
        Q_UNUSED(statusAdaptor)
        VpnControlAdaptor* controlAdaptor = new VpnControlAdaptor(service);
        QObject::connect(controlAdaptor, &VpnControlAdaptor::raiseRequested, &w, [&w]
        {
            w.showNormal();
            w.raise();
            w.activateWindow();
        });
        sessionBus.registerObject(QStringLiteral("/io/github/360900/ProtonVpnGui"), service);
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
