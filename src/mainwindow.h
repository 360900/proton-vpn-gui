#pragma once

#include <QEvent>
#include <QKeyEvent>
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
#ifdef QT_DEBUG
class DebugPage;
#endif

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    VpnManager* manager() const { return m_manager; }

private:
    enum class Page
    {
        Loading = 0,
        NotInstalled,
        Login,
        Vpn,
        Countries,
        Account,
        Settings,
#ifdef QT_DEBUG
        Debug,
#endif
    };

    VpnManager* m_manager;

    QWidget* m_sidebar;
    QStackedWidget* m_stack;

    QToolButton* m_logoBtn;
    QToolButton* m_countriesNavBtn;
    QToolButton* m_accountNavBtn;
    QToolButton* m_settingsNavBtn;
#ifdef QT_DEBUG
    QToolButton* m_debugNavBtn = nullptr;
#endif

    NotInstalledPage* m_notInstalledPage;
    LoginPage* m_loginPage;
    VpnPage* m_vpnPage;
    CountriesPage* m_countriesPage;
    AccountPage* m_accountPage;
    SettingsPage* m_settingsPage;
#ifdef QT_DEBUG
    DebugPage* m_debugPage;
#endif

    void showPage(Page page) const;
    void setupSidebar();
    void refreshIcons();
    void setNavActive(const QToolButton* btn);
    void startupCheck() const;
    void updateTrayIcon(VpnState state);
    void sendNotification(const QString& title, const QString& message) const;
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void maybeShowWhatsNew();

    QSystemTrayIcon* m_trayIcon;
    QAction* m_trayConnectAction;
    bool m_startupAutoConnectPending = false; // fire auto-connect once on first Disconnected state
    VpnState m_lastNotifiedState = VpnState::Unknown;
    bool m_whatsNewShown = false; // guard so we only show the dialog once per launch
};
