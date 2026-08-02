#pragma once
// trayController.h
// System tray icon + menu (QSystemTrayIcon lives in Qt Widgets - the one
// reason the QML app still links QApplication). Icons are rendered from the
// state SVGs at the device pixel ratio, fixing the old fixed-22px blur.

#include "../core/cliTypes.h"

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

class TrayController final : public QObject
{
    Q_OBJECT

public:
    explicit TrayController(QObject* parent = nullptr);

    void updateState(VpnState state);

    // Desktop notification via the tray (no-op when disabled in settings).
    void notify(const QString& title, const QString& message) const;

signals:
    void showRequested();
    // The user picked Quit while the VPN is active - the UI should confirm
    // (QuitDialog) before actually quitting.
    void quitConfirmationRequested();
    void connectToggleRequested();

private:
    QIcon stateIcon(VpnState state) const;

    QSystemTrayIcon* m_trayIcon;
    QMenu*           m_menu;
    QAction*         m_toggleAction = nullptr;
};
