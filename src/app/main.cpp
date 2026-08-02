// app/main.cpp
// Entry point of the QML UI. QApplication (not QGuiApplication) because the
// system tray icon and its menu live in Qt Widgets.

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QLocale>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTranslator>

#include "../appConfig.h"
#include "../core/appImageUtils.h"
#include "../core/debug.h"
#include "../core/fileLogger.h"
#include "../core/platformUtils.h"
#include "../dbus/vpnControlAdaptor.h"
#include "../dbus/vpnStatusAdaptor.h"
#include "../migrations.h"
#include "imageProviders.h"
#include "trayController.h"
#include "updateChecker.h"
#include "vpnFacade.h"

namespace
{
// How long to wait for the first instance to handle a Raise request before
// giving up (and for the lock-file fallback when D-Bus is unavailable).
constexpr int DBUS_RAISE_TIMEOUT_MS = 2'000;
constexpr int LOCK_FILE_TIMEOUT_MS  = 100;
} // namespace

int main(int argc, char* argv[])
{
    // Pin the Basic style before any QML loads: org.kde.Platform ships
    // qqc2-desktop-style which would otherwise hijack QtQuick.Controls and
    // fight our custom-drawn components.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Proton VPN GUI"));
    QApplication::setApplicationDisplayName(QStringLiteral("Proton VPN GUI"));
    // Match the installed .desktop file so the launcher groups windows
    // correctly: Flatpak renames it to the app-id, native keeps its own name.
    QApplication::setDesktopFileName(isRunningAsFlatpak()
                                         ? QStringLiteral("io.github._360900.Proton-vpn-gui")
                                         : QStringLiteral("proton-vpn-gui"));
    QApplication::setQuitOnLastWindowClosed(false); // window closes to tray

    static QTranslator translator;
    if (translator.load(QLocale::system(), QStringLiteral("proton_vpn_gui"),
                        QStringLiteral("_"), QStringLiteral(":/i18n")))
    {
        QApplication::installTranslator(&translator);
    }

    // Single source of truth for the app version.
    QString appVersion = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        appVersion = QJsonDocument::fromJson(vf.readAll()).object()
                         .value(QStringLiteral("app_version")).toString(appVersion);
    }
    QApplication::setApplicationVersion(appVersion);

    FileLogger::instance().setEnabled(AppConfig::instance().logToFile());

    DBG_APP(QStringLiteral("=== Proton VPN GUI starting (QML UI) ==="));
    DBG_APP(QStringLiteral("App version        : ") + appVersion);
    DBG_APP(QStringLiteral("Qt version         : ") + QString::fromLatin1(qVersion()));
    DBG_APP(QStringLiteral("Package type       : ") + packageTypeName());
    DBG_APP(QStringLiteral("OS                 : ") + QSysInfo::prettyProductName());
    DBG_APP(QStringLiteral("=========================================="));
    AppConfig::instance().logLoadedConfig();

    // Single-instance guard: D-Bus name ownership, lock-file fallback.
    const QString dbusServiceName = QStringLiteral("io.github._360900.Proton-vpn-gui");
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
                QStringLiteral("io.github._360900.Proton_vpn_gui.Control"),
                QStringLiteral("Raise"));
            sessionBus.call(raise, QDBus::Block, DBUS_RAISE_TIMEOUT_MS);
            return 0;
        }
    }
    else
    {
        lockFile.setStaleLockTime(0);
        if (lockFile.tryLock(LOCK_FILE_TIMEOUT_MS) == false)
        {
            QMessageBox::warning(
                nullptr,
                QCoreApplication::translate("main", "Already Running"),
                QCoreApplication::translate("main",
                    "Proton VPN GUI is already running.\n\nCheck your system tray or taskbar."));
            return 1;
        }
    }

    // Standalone AppImage vs separate host CLI conflict guard.
    if (isStandaloneAppImage() && systemProtonVpnCliInstalledSeparately())
    {
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

    Migrations::run(AppConfig::instance().lastSeenVersion());

    // The facade (and its VpnService) exists before the QML engine so the
    // D-Bus adaptors and tray can attach regardless of QML lifecycle.
    VpnFacade* facade = VpnFacade::instance();

    // Register the D-Bus object as early as possible (before QML loads) so a
    // second instance's Raise call in the single-instance check can never hit
    // an unregistered path. The raise connection is added after the window
    // exists below.
    VpnControlAdaptor* dbusControl = nullptr;
    if (sessionBus.isConnected())
    {
        const VpnStatusAdaptor* statusAdaptor = new VpnStatusAdaptor(facade->service());
        Q_UNUSED(statusAdaptor)
        dbusControl = new VpnControlAdaptor(facade->service());
        sessionBus.registerObject(QStringLiteral("/io/github/360900/ProtonVpnGui"),
                                  facade->service());
    }

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [](const QList<QQmlError>& warnings)
                     {
                         for (const QQmlError& warning : warnings)
                         {
                             DBG_APP(QStringLiteral("QML: ") + warning.toString());
                         }
                     });
    engine.addImageProvider(QStringLiteral("flag"), new FlagImageProvider());
    engine.addImageProvider(QStringLiteral("icon"), new IconImageProvider());
    engine.loadFromModule(QStringLiteral("ProtonVpnGui"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
    {
        DBG_APP(QStringLiteral("FATAL: QML root failed to load (see QML warnings above)."));
        return 1;
    }
    QQuickWindow* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());

    // System tray.
    TrayController tray;
    QObject::connect(&tray, &TrayController::showRequested, window, [window]
    {
        if (window != nullptr)
        {
            window->show();
            window->raise();
            window->requestActivate();
        }
    });
    QObject::connect(&tray, &TrayController::connectToggleRequested,
                     facade, &VpnFacade::togglePower);
    QObject::connect(&tray, &TrayController::quitConfirmationRequested, window, [window]
    {
        if (window != nullptr)
        {
            window->show();
            window->raise();
            QMetaObject::invokeMethod(window, "openQuitDialog");
        }
    });
    // Track the previous state so the first stateChanged (which just reveals
    // the state the app launched into) does not fire a notification.
    VpnState lastTrayState = VpnState::Unknown;
    QObject::connect(facade->service(), &VpnService::stateChanged, &tray,
                     [&tray, facade, &lastTrayState](const VpnState state, const QString&)
                     {
                         tray.updateState(state);
                         const bool firstReveal = lastTrayState == VpnState::Unknown &&
                                                  state != VpnState::Unknown;
                         lastTrayState = state;
                         if (firstReveal)
                         {
                             return; // startup reveal - no notification
                         }
                         if (state == VpnState::Connected)
                         {
                             const QString server = facade->service()->connectedServer();
                             tray.notify(QObject::tr("Proton VPN"),
                                         server.isEmpty()
                                             ? QObject::tr("Connected.")
                                             : QObject::tr("Connected to %1.").arg(server));
                         }
                         else if (state == VpnState::Disconnected)
                         {
                             tray.notify(QObject::tr("Proton VPN"),
                                         QObject::tr("Disconnected."));
                         }
                     });

    // Update check + post-update What's New note.
    UpdateChecker updateChecker;
    if (window != nullptr)
    {
        QObject::connect(&updateChecker, &UpdateChecker::updateAvailable, window,
                         [window](const QString& current, const QString& next)
                         {
                             QMetaObject::invokeMethod(window, "openUpdateDialog",
                                                       Q_ARG(QVariant, current),
                                                       Q_ARG(QVariant, next));
                         });
        const QString lastSeen = AppConfig::instance().lastSeenVersion();
        if (lastSeen.isEmpty() == false && lastSeen != appVersion)
        {
            QMetaObject::invokeMethod(window, "openWhatsNewDialog",
                                      Q_ARG(QVariant, appVersion),
                                      Qt::QueuedConnection);
        }
    }
    AppConfig::instance().setLastSeenVersion(appVersion);
    updateChecker.checkSoon();

    // D-Bus: the object path was registered above; only attach the window
    // raise wiring now that the window exists.
    if (sessionBus.isConnected() && window != nullptr && dbusControl != nullptr)
    {
        QObject::connect(dbusControl, &VpnControlAdaptor::raiseRequested, window,
                         [window]
                         {
                             window->show();
                             window->raise();
                             window->requestActivate();
                         });
    }

    if (window != nullptr && AppConfig::instance().startHidden() == false)
    {
        window->show();
    }

    facade->startup();
    return QApplication::exec();
}
