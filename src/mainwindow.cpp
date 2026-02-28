#include "mainwindow.h"

#include <QApplication>
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
#include "pages/settingspage.h"
#include "appconfig.h"

static QIcon svgNavIcon(const QString &path, const QSize &size = {24, 24}, bool tintInDarkMode = true)
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);

    // Tint white in dark mode so monochrome icons are visible on the dark sidebar.
    // Pass tintInDarkMode=false for logos/icons that carry their own colors.
    if (tintInDarkMode) {
        const QColor windowColor = QApplication::palette().color(QPalette::Window);
        if (windowColor.lightness() < 128) {
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pix.rect(), Qt::white);
        }
    }

    p.end();
    return QIcon(pix);
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("ProtonVPN"));
    setWindowIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {64, 64}, false));
    setMinimumSize(490, 560);
    resize(490, 600);

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
    m_sidebar->setEnabled(false);  // disabled until startup checks complete
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

    // Settings page (index 6)
    m_settingsPage = new SettingsPage(m_manager);
    m_stack->addWidget(m_settingsPage);  // index 6

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
            // Mark that we should auto-connect once the initial status check resolves.
            if (AppConfig::instance().autoConnect())
                m_startupAutoConnectPending = true;
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
        updateTrayIcon(state);

        // Auto-connect: if we flagged a pending connect and we just confirmed
        // the VPN is disconnected, initiate the connection now.
        if (m_startupAutoConnectPending && state == VpnState::Disconnected) {
            m_startupAutoConnectPending = false;
            m_manager->connectVpn();
        }
    });

    // System tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    auto *trayMenu = new QMenu(this);
    trayMenu->addAction(QStringLiteral("Show"), this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addSeparator();
    m_trayConnectAction = trayMenu->addAction(QStringLiteral("Connect"), this, [this]() {
        const VpnState state = m_manager->currentState();
        if (state == VpnState::Connected)
            m_manager->disconnectVpn();
        else if (state == VpnState::Disconnected || state == VpnState::Error)
            m_manager->connectVpn();
    });
    trayMenu->addSeparator();
    trayMenu->addAction(QStringLiteral("Quit"), qApp, &QApplication::quit);
    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    updateTrayIcon(VpnState::Unknown);
    m_trayIcon->show();

    // Start
    showPage(Page::Loading);
    startupCheck();
}

void MainWindow::setupSidebar()
{
    auto *layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(4);

    // Logo at top — use a QLabel with a transparent pixmap so no background shows
    auto *logoLabel = new QLabel(m_sidebar);
    logoLabel->setPixmap(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {40, 40}, false).pixmap(40, 40));
    logoLabel->setFixedSize(40, 40);
    logoLabel->setAttribute(Qt::WA_TranslucentBackground);
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
    m_accountNavBtn = makeNavBtn(QStringLiteral("Account"), QStringLiteral(":/assets/person-lines-fill.svg"));

    connect(m_vpnNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Vpn); });
    connect(m_countriesNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Countries); });
    connect(m_accountNavBtn, &QToolButton::clicked, this, [this]() {
        showPage(Page::Account);
        m_accountPage->refresh();
    });

    layout->addStretch();

    // Settings button pinned to the bottom of the sidebar
    m_settingsNavBtn = makeNavBtn(QStringLiteral("Settings"), QStringLiteral(":/assets/gear.svg"));
    connect(m_settingsNavBtn, &QToolButton::clicked, this, [this]() {
        showPage(Page::Settings);
        m_settingsPage->refresh();
    });
}

void MainWindow::showPage(Page page)
{
    m_stack->setCurrentIndex(static_cast<int>(page));

    m_vpnNavBtn->setChecked(page == Page::Vpn);
    m_countriesNavBtn->setChecked(page == Page::Countries);
    m_accountNavBtn->setChecked(page == Page::Account);
    m_settingsNavBtn->setChecked(page == Page::Settings);
}

void MainWindow::setNavActive(QToolButton *btn)
{
    for (auto *b : {m_vpnNavBtn, m_countriesNavBtn, m_accountNavBtn, m_settingsNavBtn})
        b->setChecked(b == btn);
}

void MainWindow::startupCheck()
{
    m_manager->checkInstalled();
}

void MainWindow::updateTrayIcon(VpnState state)
{
    // Choose asset based on state
    QString asset;
    switch (state) {
    case VpnState::Connected:
        asset = QStringLiteral(":/assets/state-connected.svg");    break;
    case VpnState::Connecting:
    case VpnState::Disconnecting:
        asset = QStringLiteral(":/assets/state-connecting.svg");   break;
    case VpnState::Error:
        asset = QStringLiteral(":/assets/state-error.svg");        break;
    default:
        asset = QStringLiteral(":/assets/state-disconnected.svg"); break;
    }

    // Render into a pixmap (use a larger size for the window icon, smaller for tray)
    auto makeIcon = [&](int sz) {
        QPixmap pix(sz, sz);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        QSvgRenderer renderer(asset);
        renderer.render(&p);
        return QIcon(pix);
    };

    const QIcon icon = makeIcon(64);
    m_trayIcon->setIcon(makeIcon(22));
    setWindowIcon(icon);

    switch (state) {
    case VpnState::Connected:
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Connected"));
        m_trayConnectAction->setText(QStringLiteral("Disconnect"));
        m_trayConnectAction->setEnabled(true);
        break;
    case VpnState::Connecting:
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Connecting…"));
        m_trayConnectAction->setText(QStringLiteral("Connecting…"));
        m_trayConnectAction->setEnabled(false);
        break;
    case VpnState::Disconnecting:
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Disconnecting…"));
        m_trayConnectAction->setText(QStringLiteral("Disconnecting…"));
        m_trayConnectAction->setEnabled(false);
        break;
    case VpnState::Error:
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Error"));
        m_trayConnectAction->setText(QStringLiteral("Connect"));
        m_trayConnectAction->setEnabled(true);
        break;
    case VpnState::Disconnected:
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Disconnected"));
        m_trayConnectAction->setText(QStringLiteral("Connect"));
        m_trayConnectAction->setEnabled(true);
        break;
    default:  // Unknown — still checking
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Checking…"));
        m_trayConnectAction->setText(QStringLiteral("Connect"));
        m_trayConnectAction->setEnabled(false);
        break;
    }
}

