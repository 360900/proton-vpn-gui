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

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ProtonVPN"));
    QApplication::setApplicationDisplayName(QStringLiteral("ProtonVPN"));

    // Read version from the embedded version.json so there is a single source of truth.
    QString appVersion = QStringLiteral("1.0.0");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
            appVersion = obj[QStringLiteral("app_version")].toString();
    }
    QApplication::setApplicationVersion(appVersion);

    // Single-instance guard — prevent multiple copies running at the same time.
    const QString lockPath = QDir::tempPath() + QStringLiteral("/proton-vpn-qt-app.lock");
    QLockFile lockFile(lockPath);
    lockFile.setStaleLockTime(0); // never treat a lock as stale
    if (!lockFile.tryLock(100))
    {
        QMessageBox::warning(
            nullptr,
            QStringLiteral("Already Running"),
            QStringLiteral("ProtonVPN is already running.\n\nCheck your system tray or taskbar."));
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

    // Application-wide stylesheet
    app.setStyleSheet(QStringLiteral(R"(
        QWidget {
            background-color: #1a1a2e;
            color: #eaeaea;
            font-family: "Inter", "Noto Sans", "Segoe UI", sans-serif;
            font-size: 13px;
        }

        QLabel {
            background-color: transparent;
        }

        QWidget#sidebar {
            background-color: #131320;
            border-right: 1px solid #3a3a55;
        }

        QFrame#sidebarDivider {
            color: #3a3a55;
        }

        QToolButton#navButton {
            background-color: transparent;
            border: none;
            border-radius: 8px;
            padding: 6px;
        }
        QToolButton#navButton:hover {
            background-color: #2d2d4a;
        }
        QToolButton#navButton:checked {
            background-color: #3a2d7a;
        }

        QWidget#loginCard {
            background-color: #25253d;
            border-radius: 12px;
            border: 1px solid #3a3a55;
        }

        QLabel#sectionTitle {
            font-size: 18px;
            font-weight: bold;
            color: #eaeaea;
        }

        QLabel#fieldLabel {
            color: #9999bb;
            font-size: 12px;
        }

        QLabel#errorLabel {
            color: #ff6b6b;
            background-color: rgba(214, 63, 63, 0.1);
            border: 1px solid #d63f3f;
            border-radius: 6px;
            padding: 8px;
        }

        QLabel#timerLabel {
            font-size: 16px;
            color: #9999bb;
            font-family: "Courier New", "DejaVu Sans Mono", monospace;
        }

        QLabel#infoLabel {
            color: #9999bb;
            font-size: 12px;
        }

        QLineEdit#inputField {
            background-color: #1a1a2e;
            border: 1px solid #3a3a55;
            border-radius: 6px;
            padding: 8px 12px;
            color: #eaeaea;
            selection-background-color: #6d4aff;
        }
        QLineEdit#inputField:focus {
            border-color: #6d4aff;
        }

        QPushButton#primaryButton {
            background-color: #6d4aff;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 24px;
            font-weight: bold;
            font-size: 14px;
            min-width: 140px;
        }
        QPushButton#primaryButton:hover {
            background-color: #5a3de0;
        }
        QPushButton#primaryButton:disabled {
            background-color: #3a3a55;
            color: #666680;
        }

        QPushButton#secondaryButton {
            background-color: transparent;
            color: #9999bb;
            border: 1px solid #3a3a55;
            border-radius: 6px;
            padding: 4px 12px;
        }
        QPushButton#secondaryButton:hover {
            background-color: #2d2d4a;
            color: #eaeaea;
        }

        QPushButton#dangerButton {
            background-color: transparent;
            color: #ff6b6b;
            border: 1px solid #d63f3f;
            border-radius: 6px;
            padding: 10px 24px;
            font-weight: bold;
        }
        QPushButton#dangerButton:hover {
            background-color: rgba(214, 63, 63, 0.15);
        }

        QPushButton#iconButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
        }
        QPushButton#iconButton:hover {
            background-color: #2d2d4a;
        }

        QListWidget#serverList {
            background-color: #1a1a2e;
            border: 1px solid #3a3a55;
            border-radius: 6px;
            outline: none;
        }
        QListWidget#serverList::item {
            padding: 8px 12px;
            border-bottom: 1px solid #2d2d4a;
        }
        QListWidget#serverList::item:selected {
            background-color: #3a2d7a;
            color: white;
        }
        QListWidget#serverList::item:hover {
            background-color: #2d2d4a;
        }

        QLabel#listHeader {
            font-size: 12px;
            font-weight: bold;
            color: #9999bb;
            text-transform: uppercase;
            letter-spacing: 1px;
            padding: 4px 0;
        }

        QWidget#infoCard {
            background-color: #25253d;
            border-radius: 8px;
            border: 1px solid #3a3a55;
        }

        QWidget#infoRow {
            background-color: transparent;
        }

        QLabel#infoKey {
            color: #9999bb;
        }

        QLabel#infoValue {
            color: #eaeaea;
            font-weight: bold;
        }

        QFrame#divider {
            color: #3a3a55;
        }

        QSplitter::handle {
            background-color: #3a3a55;
            width: 1px;
        }

        QScrollBar:vertical {
            background: #1a1a2e;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #3a3a55;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #6d4aff;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )"));

    MainWindow w;
    w.show();
    return QApplication::exec();
}

