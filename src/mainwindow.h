#pragma once

#include <QStackedWidget>
#include <QToolButton>
#include <QSystemTrayIcon>
#include <QMenu>
#include "vpnmanager.h"

class VpnPage;
class LoginPage;
class CountriesPage;
class AccountPage;
class NotInstalledPage;
class SettingsPage;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    enum class Page
    {
        Loading = 0,
        NotInstalled,
        Login,
        Vpn,
        Countries,
        Account,
        Settings
    };

    VpnManager* m_manager;

    QWidget* m_sidebar;
    QStackedWidget* m_stack;

    QToolButton* m_vpnNavBtn;
    QToolButton* m_countriesNavBtn;
    QToolButton* m_accountNavBtn;
    QToolButton* m_settingsNavBtn;

    NotInstalledPage* m_notInstalledPage;
    LoginPage* m_loginPage;
    VpnPage* m_vpnPage;
    CountriesPage* m_countriesPage;
    AccountPage* m_accountPage;
    SettingsPage* m_settingsPage;

    void showPage(Page page) const;
    void setupSidebar();
    void setNavActive(QToolButton* btn);
    void startupCheck() const;
    void updateTrayIcon(VpnState state);
    void sendNotification(const QString& title, const QString& message) const;

    QSystemTrayIcon* m_trayIcon;
    QAction* m_trayConnectAction;
    bool m_startupAutoConnectPending = false; // fire auto-connect once on first Disconnected state
    VpnState m_lastNotifiedState = VpnState::Unknown;
};

