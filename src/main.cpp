#include <QApplication>
#include <QStyleFactory>
#include <QColor>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include "mainwindow.h"
#include "appconfig.h"
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


    // Dark Proton-branded palette
    QPalette palette;
    constexpr QColor bg(0x1a, 0x1a, 0x2e); // deep navy background
    constexpr QColor surface(0x25, 0x25, 0x3d); // card surface
    constexpr QColor border(0x3a, 0x3a, 0x55); // borders
    constexpr QColor accent(0x6d, 0x4a, 0xff); // Proton purple
    constexpr QColor textPrimary(0xea, 0xea, 0xea); // near-white text
    constexpr QColor textSecondary(0x99, 0x99, 0xbb);
    constexpr QColor highlight(0x6d, 0x4a, 0xff);

    palette.setColor(QPalette::Window, bg);
    palette.setColor(QPalette::WindowText, textPrimary);
    palette.setColor(QPalette::Base, surface);
    palette.setColor(QPalette::AlternateBase, bg);
    palette.setColor(QPalette::ToolTipBase, surface);
    palette.setColor(QPalette::ToolTipText, textPrimary);
    palette.setColor(QPalette::Text, textPrimary);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::ButtonText, textPrimary);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, textSecondary);
    palette.setColor(QPalette::Mid, border);
    palette.setColor(QPalette::Dark, border);
    palette.setColor(QPalette::Midlight, surface);
    palette.setColor(QPalette::Shadow, QColor(0x00, 0x00, 0x00, 0x80));
    QApplication::setPalette(palette);

    // Application-wide stylesheet – defined in style.qss (embedded via resources.qrc)
    QFile qssFile(QStringLiteral(":/style.qss"));
    if (qssFile.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(QString::fromUtf8(qssFile.readAll()));

    MainWindow w;
    if (AppConfig::instance().startHidden())
        w.hide();
    else
        w.show();
    return QApplication::exec();
}

