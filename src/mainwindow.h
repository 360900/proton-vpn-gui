#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QToolButton>
#include "vpnmanager.h"

class VpnPage;
class LoginPage;
class CountriesPage;
class AccountPage;
class NotInstalledPage;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    enum class Page {
        Loading = 0,
        NotInstalled,
        Login,
        Vpn,
        Countries,
        Account
    };

    VpnManager *m_manager;

    QWidget *m_sidebar;
    QStackedWidget *m_stack;

    QToolButton *m_vpnNavBtn;
    QToolButton *m_countriesNavBtn;
    QToolButton *m_accountNavBtn;

    NotInstalledPage *m_notInstalledPage;
    LoginPage *m_loginPage;
    VpnPage *m_vpnPage;
    CountriesPage *m_countriesPage;
    AccountPage *m_accountPage;

    void showPage(Page page);
    void setupSidebar();
    void setNavActive(QToolButton *btn);
    void startupCheck();
};

