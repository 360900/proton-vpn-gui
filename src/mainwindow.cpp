#include "mainwindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QSvgWidget>
#include <QToolButton>
#include <QButtonGroup>
#include <QFrame>

#include "pages/notinstalledpage.h"
#include "pages/loginpage.h"
#include "pages/vpnpage.h"
#include "pages/countriespage.h"
#include "pages/accountpage.h"

static QIcon svgNavIcon(const QString &path, const QSize &size = {24, 24})
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);
    return QIcon(pix);
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("ProtonVPN"));
    setMinimumSize(480, 560);
    resize(480, 600);

    m_manager = new VpnManager(this);

    // Root layout: sidebar + content
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(64);
    setupSidebar();
    rootLayout->addWidget(m_sidebar);

    // Vertical divider
    auto *divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setObjectName(QStringLiteral("sidebarDivider"));
    rootLayout->addWidget(divider);

    // Stacked content
    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // Loading page (index 0)
    auto *loadingPage = new QWidget();
    auto *loadingLayout = new QVBoxLayout(loadingPage);
    loadingLayout->setAlignment(Qt::AlignCenter);
    auto *loadingLabel = new QLabel(QStringLiteral("Starting…"), loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel);
    m_stack->addWidget(loadingPage);  // index 0 = Loading

    // Not installed page (index 1)
    m_notInstalledPage = new NotInstalledPage();
    m_stack->addWidget(m_notInstalledPage);  // index 1

    // Login page (index 2)
    m_loginPage = new LoginPage();
    m_stack->addWidget(m_loginPage);  // index 2
    connect(m_loginPage, &LoginPage::loginRequested, this, [this](const QString &u, const QString &p) {
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
        m_manager->login(u, p);
    });
    connect(m_loginPage, &LoginPage::twoFASubmitted, this, [this](const QString &token) {
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
        m_manager->submit2FA(token);
    });

    // VPN page (index 3)
    m_vpnPage = new VpnPage(m_manager);
    m_stack->addWidget(m_vpnPage);  // index 3
    connect(m_vpnPage, &VpnPage::connectRequested, m_manager, [this]() {
        m_manager->connectVpn();
    });
    connect(m_vpnPage, &VpnPage::disconnectRequested, m_manager, &VpnManager::disconnectVpn);

    // Countries page (index 4)
    m_countriesPage = new CountriesPage(m_manager);
    m_stack->addWidget(m_countriesPage);  // index 4
    connect(m_countriesPage, &CountriesPage::connectRequested, this,
            [this](const QString &country, const QString &city) {
        m_manager->connectVpn(country, city);
        showPage(Page::Vpn);
    });

    // Account page (index 5)
    m_accountPage = new AccountPage(m_manager);
    m_stack->addWidget(m_accountPage);  // index 5
    connect(m_accountPage, &AccountPage::signOutRequested, this, [this]() {
        m_manager->signOut();
    });

    // VpnManager signals
    connect(m_manager, &VpnManager::installedResult, this, [this](bool installed) {
        if (!installed) {
            m_sidebar->setEnabled(false);
            showPage(Page::NotInstalled);
        } else {
            m_manager->checkLoginStatus();
        }
    });

    connect(m_manager, &VpnManager::loginStatusResult, this, [this](bool loggedIn, const QString &username) {
        Q_UNUSED(username)
        if (loggedIn) {
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->fetchCountries();
        } else {
            m_sidebar->setEnabled(false);
            showPage(Page::Login);
        }
    });

    connect(m_manager, &VpnManager::twoFactorRequired, this, [this]() {
        m_loginPage->setLoading(false);
        m_loginPage->show2FAPrompt();
    });

    connect(m_manager, &VpnManager::loginFinished, this, [this](bool ok, const QString &error) {
        m_loginPage->setLoading(false);
        if (ok) {
            m_loginPage->reset();
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->checkConnectionStatus();
            m_manager->fetchCountries();
        } else {
            m_loginPage->setError(error.isEmpty()
                ? QStringLiteral("Login failed. Please check your credentials.")
                : error);
        }
    });

    connect(m_manager, &VpnManager::signOutFinished, this, [this](bool) {
        m_loginPage->reset();
        m_sidebar->setEnabled(false);
        showPage(Page::Login);
    });

    connect(m_manager, &VpnManager::connectionStateChanged, this,
            [this](VpnState state, const QString &info) {
        m_vpnPage->onStateChanged(state, info);
    });

    // Start
    showPage(Page::Loading);
    startupCheck();
}

void MainWindow::setupSidebar()
{
    auto *layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(4);

    // Logo at top
    auto *logoLabel = new QSvgWidget(QStringLiteral(":/assets/proton-vpn-sign.svg"), m_sidebar);
    logoLabel->setFixedSize(40, 40);
    layout->addWidget(logoLabel, 0, Qt::AlignHCenter);

    layout->addSpacing(12);

    auto *btnGroup = new QButtonGroup(m_sidebar);
    btnGroup->setExclusive(true);

    auto makeNavBtn = [&](const QString &tooltip, const QString &iconPath) -> QToolButton* {
        auto *btn = new QToolButton(m_sidebar);
        btn->setToolTip(tooltip);
        btn->setIcon(svgNavIcon(iconPath, {24, 24}));
        btn->setIconSize({24, 24});
        btn->setFixedSize(48, 48);
        btn->setCheckable(true);
        btn->setObjectName(QStringLiteral("navButton"));
        btn->setCursor(Qt::PointingHandCursor);
        btnGroup->addButton(btn);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
        return btn;
    };

    m_vpnNavBtn = makeNavBtn(QStringLiteral("VPN"), QStringLiteral(":/assets/state-disconnected.svg"));
    m_countriesNavBtn = makeNavBtn(QStringLiteral("Countries"), QStringLiteral(":/assets/server-smart-routing.svg"));
    m_accountNavBtn = makeNavBtn(QStringLiteral("Account"), QStringLiteral(":/assets/security-key.svg"));

    connect(m_vpnNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Vpn); });
    connect(m_countriesNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Countries); });
    connect(m_accountNavBtn, &QToolButton::clicked, this, [this]() {
        showPage(Page::Account);
        m_accountPage->refresh();
    });

    layout->addStretch();
}

void MainWindow::showPage(Page page)
{
    m_stack->setCurrentIndex(static_cast<int>(page));

    // Update nav highlights
    m_vpnNavBtn->setChecked(page == Page::Vpn);
    m_countriesNavBtn->setChecked(page == Page::Countries);
    m_accountNavBtn->setChecked(page == Page::Account);
}

void MainWindow::setNavActive(QToolButton *btn)
{
    for (auto *b : {m_vpnNavBtn, m_countriesNavBtn, m_accountNavBtn}) {
        b->setChecked(b == btn);
    }
}

void MainWindow::startupCheck()
{
    m_manager->checkInstalled();
}

