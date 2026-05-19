#include <QApplication>
#include <QStyleFactory>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include "mainwindow.h"
#include "appconfig.h"
#include "thememanager.h"
#include "debug.h"
#include "cli/flatpakutils.h"
#include <QSysInfo>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ProtonVPN"));
    QApplication::setApplicationDisplayName(QStringLiteral("ProtonVPN"));

    // Install translator for UI strings
    static QTranslator translator;
    const QLocale locale = QLocale::system();
    if (translator.load(locale, QStringLiteral("proton_vpn_qt"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(&translator);

    // Read version from the embedded version.json so there is a single source of truth.
    QString appVersion = QStringLiteral("unknown");
    QString cliVersionTested = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
            appVersion = obj[QStringLiteral("app_version")].toString();
        if (obj.contains(QStringLiteral("cli_version_tested")))
            cliVersionTested = obj[QStringLiteral("cli_version_tested")].toString();
    }
    QApplication::setApplicationVersion(appVersion);

    // ── Startup diagnostics ──────────────────────────────────────────────────
    DBG_APP(QStringLiteral("=== ProtonVPN Qt App starting ==="));
    DBG_APP(QStringLiteral("App version    : ") + appVersion);
    DBG_APP(QStringLiteral("CLI tested for : ") + cliVersionTested);
    DBG_APP(QStringLiteral("Qt version     : ") + QString::fromLatin1(qVersion()));
    DBG_APP(QStringLiteral("Package type   : ") + (isRunningAsFlatpak() ? QStringLiteral("Flatpak") : QStringLiteral("System")));
    DBG_APP(QStringLiteral("OS             : ") + QSysInfo::prettyProductName());
    DBG_APP(QStringLiteral("Kernel         : ") + QSysInfo::kernelVersion());
    DBG_APP(QStringLiteral("CPU arch       : ") + QSysInfo::currentCpuArchitecture());
    DBG_APP(QStringLiteral("Locale         : ") + QLocale::system().name());
    DBG_APP(QStringLiteral("Config dir     : ") +
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    DBG_APP(QStringLiteral("================================="));

    // Single-instance guard — prevent multiple copies running at the same time.
    const QString lockPath = QDir::tempPath() + QStringLiteral("/proton-vpn-qt-app.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0); // never treat a lock as stale
    if (!lockFile.tryLock(100))
    {
        qint64 pid = 0;
        QString hostname, appname;
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

    MainWindow w;
    if (AppConfig::instance().startHidden())
        w.hide();
    else
        w.show();
    return QApplication::exec();
}

