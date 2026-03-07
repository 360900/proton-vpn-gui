#include "mainwindow.h"

#include <QApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QPushButton>
#include <QSvgRenderer>
#include <QSvgWidget>
#include <QToolButton>
#include <QButtonGroup>
#include <QFrame>
#include <QVBoxLayout>

#include "pages/notinstalledpage.h"
#include "pages/loginpage.h"
#include "pages/vpnpage.h"
#include "pages/countriespage.h"
#include "pages/accountpage.h"
#include "pages/settingspage.h"
#include "appconfig.h"

static QIcon svgNavIcon(const QString& path, const QSize& size = {24, 24}, bool tintInDarkMode = true)
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);

    // Tint white in dark mode so monochrome icons are visible on the dark sidebar.
    // Pass tintInDarkMode=false for logos/icons that carry their own colors.
    if (tintInDarkMode)
    {
        const QColor windowColor = QApplication::palette().color(QPalette::Window);
        if (windowColor.lightness() < 128)
        {
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(pix.rect(), Qt::white);
        }
    }

    p.end();
    return QIcon(pix);
}

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("ProtonVPN"));
    setWindowIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {64, 64}, false));
    setMinimumSize(490, 560);
    resize(530, 600);

    m_manager = new VpnManager(this);

    // Root layout: sidebar + content
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(64);
    setupSidebar();
    m_sidebar->setEnabled(false); // disabled until startup checks complete
    rootLayout->addWidget(m_sidebar);

    // Vertical divider
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setObjectName(QStringLiteral("sidebarDivider"));
    rootLayout->addWidget(divider);

    // Stacked content
    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // Loading page (index 0)
    auto* loadingPage = new QWidget();
    auto* loadingLayout = new QVBoxLayout(loadingPage);
    loadingLayout->setAlignment(Qt::AlignCenter);
    auto* loadingLabel = new QLabel(QStringLiteral("Starting…"), loadingPage);
    loadingLabel->setAlignment(Qt::AlignCenter);
    loadingLayout->addWidget(loadingLabel);
    m_stack->addWidget(loadingPage); // index 0 = Loading

    // Not installed page (index 1)
    m_notInstalledPage = new NotInstalledPage();
    m_stack->addWidget(m_notInstalledPage); // index 1

    // Login page (index 2)
    m_loginPage = new LoginPage();
    m_stack->addWidget(m_loginPage); // index 2
    connect(m_loginPage, &LoginPage::loginRequested, this, [this](const QString& u, const QString& p)
    {
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
        m_manager->login(u, p);
    });
    connect(m_loginPage, &LoginPage::twoFASubmitted, this, [this](const QString& token)
    {
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
        m_manager->submit2FA(token);
    });

    // VPN page (index 3)
    m_vpnPage = new VpnPage(m_manager);
    m_stack->addWidget(m_vpnPage); // index 3
    connect(m_vpnPage, &VpnPage::connectRequested, m_manager,
            [this](const QString& country, const QString& city)
            {
                m_manager->connectVpn(country, city);
            });
    connect(m_vpnPage, &VpnPage::disconnectRequested, m_manager, &VpnManager::disconnectVpn);

    // Countries page (index 4)
    m_countriesPage = new CountriesPage(m_manager);
    m_stack->addWidget(m_countriesPage); // index 4
    connect(m_countriesPage, &CountriesPage::connectRequested, this,
            [this](const QString& country, const QString& city)
            {
                m_manager->connectVpn(country, city);
                showPage(Page::Vpn);
            });

    // Account page (index 5)
    m_accountPage = new AccountPage(m_manager);
    m_stack->addWidget(m_accountPage); // index 5
    connect(m_accountPage, &AccountPage::signOutRequested, this, [this]()
    {
        m_manager->signOut();
    });

    // Settings page (index 6)
    m_settingsPage = new SettingsPage(m_manager);
    m_stack->addWidget(m_settingsPage); // index 6

    // VpnManager signals
    connect(m_manager, &VpnManager::installedResult, this, [this](bool installed)
    {
        if (!installed)
        {
            m_sidebar->setEnabled(false);
            showPage(Page::NotInstalled);
        }
        else
        {
            m_manager->checkLoginStatus();
        }
    });

    connect(m_manager, &VpnManager::loginStatusResult, this, [this](bool loggedIn, const QString& username)
    {
        Q_UNUSED(username)
        if (loggedIn)
        {
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->fetchCountries();
            // Mark that we should auto-connect once the initial status check resolves.
            if (AppConfig::instance().autoConnect())
                m_startupAutoConnectPending = true;
        }
        else
        {
            m_sidebar->setEnabled(false);
            showPage(Page::Login);
        }
    });

    connect(m_manager, &VpnManager::twoFactorRequired, this, [this]()
    {
        m_loginPage->setLoading(false);
        m_loginPage->show2FAPrompt();
    });

    connect(m_manager, &VpnManager::loginFinished, this, [this](bool ok, const QString& error)
    {
        m_loginPage->setLoading(false);
        if (ok)
        {
            m_loginPage->reset();
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->checkConnectionStatus();
            m_manager->fetchCountries();
        }
        else
        {
            m_loginPage->setError(error.isEmpty()
                                      ? QStringLiteral("Login failed. Please check your credentials.")
                                      : error);
        }
    });

    connect(m_manager, &VpnManager::signOutFinished, this, [this](bool)
    {
        m_loginPage->reset();
        m_sidebar->setEnabled(false);
        showPage(Page::Login);
    });

    connect(m_manager, &VpnManager::connectionStateChanged, this,
            [this](VpnState state, const QString& info)
            {
                m_vpnPage->onStateChanged(state, info);
                updateTrayIcon(state);

                // Auto-connect: if we flagged a pending connect and we just confirmed
                // the VPN is disconnected, initiate the connection now.
                if (m_startupAutoConnectPending && state == VpnState::Disconnected)
                {
                    m_startupAutoConnectPending = false;
                    m_manager->connectVpn();
                }
            });

    // System tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    auto* trayMenu = new QMenu(this);
    trayMenu->addAction(QStringLiteral("Show"), this, [this]()
    {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addSeparator();
    m_trayConnectAction = trayMenu->addAction(QStringLiteral("Connect"), this, [this]()
    {
        const VpnState state = m_manager->currentState();
        if (state == VpnState::Connected)
            m_manager->disconnectVpn();
        else if (state == VpnState::Disconnected || state == VpnState::Error)
            m_manager->connectVpn();
    });
    trayMenu->addSeparator();
    trayMenu->addAction(QStringLiteral("Quit"), this, [this]()
    {
        // Only prompt when the VPN is active
        if (m_manager->currentState() != VpnState::Connected &&
            m_manager->currentState() != VpnState::Connecting)
        {
            QApplication::quit();
            return;
        }

        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(QStringLiteral("Quit ProtonVPN"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setModal(true);
        dlg->setMinimumWidth(440);

        auto* layout = new QVBoxLayout(dlg);
        layout->setSpacing(16);
        layout->setContentsMargins(24, 24, 24, 20);

        auto* msgLabel = new QLabel(
            QStringLiteral("The VPN is currently active.<br>"
                           "What would you like to do before quitting?"), dlg);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        layout->addWidget(msgLabel);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(8);

        auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), dlg);
        cancelBtn->setObjectName(QStringLiteral("secondaryButton"));

        auto* leaveOnBtn = new QPushButton(QStringLiteral("Leave VPN on"), dlg);
        leaveOnBtn->setObjectName(QStringLiteral("leaveVpnOnButton"));
        leaveOnBtn->setDefault(true);

        auto* disconnectBtn = new QPushButton(QStringLiteral("Disconnect VPN"), dlg);
        disconnectBtn->setObjectName(QStringLiteral("dangerButton"));

        // Uniform size: same height, equal width via stretch, reduced horizontal
        // padding so the longest label ("Disconnect VPN") fits without clipping.
        const QString overridePadding = QStringLiteral("padding-left: 8px; padding-right: 8px;");
        cancelBtn->setStyleSheet(
            QStringLiteral("QPushButton#secondaryButton { %1 }").arg(overridePadding));
        leaveOnBtn->setStyleSheet(
            QStringLiteral("QPushButton#leaveVpnOnButton { %1 }").arg(overridePadding));
        disconnectBtn->setStyleSheet(
            QStringLiteral("QPushButton#dangerButton { %1 }").arg(overridePadding));

        const int btnH = disconnectBtn->sizeHint().height();
        cancelBtn->setFixedHeight(btnH);
        leaveOnBtn->setFixedHeight(btnH);
        disconnectBtn->setFixedHeight(btnH);

        btnRow->addWidget(cancelBtn, 1);
        btnRow->addWidget(leaveOnBtn, 1);
        btnRow->addWidget(disconnectBtn, 1);
        layout->addLayout(btnRow);

        connect(cancelBtn,    &QPushButton::clicked, dlg, &QDialog::reject);
        connect(leaveOnBtn,   &QPushButton::clicked, dlg, &QDialog::accept);
        connect(disconnectBtn, &QPushButton::clicked, dlg, [dlg]()
        {
            dlg->done(2); // custom result code for "disconnect then quit"
        });

        const int result = dlg->exec();
        if (result == QDialog::Rejected)
            return; // user cancelled — do nothing

        if (result == 2)
            m_manager->disconnectVpnSync(); // blocks until protonvpn disconnect finishes

        QApplication::quit();
    });
    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger)
                {
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
    auto* layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(4);

    auto* btnGroup = new QButtonGroup(m_sidebar);
    btnGroup->setExclusive(true);

    // Logo button is part of the exclusive group so clicking it automatically
    // clears the checked state on all nav buttons.
    m_logoBtn = new QToolButton(m_sidebar);
    m_logoBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {40, 40}, false));
    m_logoBtn->setIconSize({40, 40});
    m_logoBtn->setFixedSize(40, 40);
    m_logoBtn->setToolTip(QStringLiteral("VPN"));
    m_logoBtn->setCursor(Qt::PointingHandCursor);
    m_logoBtn->setObjectName(QStringLiteral("logoButton"));
    m_logoBtn->setCheckable(true);
    btnGroup->addButton(m_logoBtn);
    layout->addWidget(m_logoBtn, 0, Qt::AlignHCenter);
    connect(m_logoBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Vpn); });

    layout->addSpacing(12);

    auto makeNavBtn = [&](const QString& tooltip, const QString& iconPath) -> QToolButton*
    {
        auto* btn = new QToolButton(m_sidebar);
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

    m_countriesNavBtn = makeNavBtn(QStringLiteral("Countries"), QStringLiteral(":/assets/server-smart-routing.svg"));
    m_accountNavBtn = makeNavBtn(QStringLiteral("Account"), QStringLiteral(":/assets/person-lines-fill.svg"));

    connect(m_countriesNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Countries); });
    connect(m_accountNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Account);
        m_accountPage->refresh();
    });

    layout->addStretch();

    // Settings button pinned to the bottom of the sidebar
    m_settingsNavBtn = makeNavBtn(QStringLiteral("Settings"), QStringLiteral(":/assets/gear.svg"));
    connect(m_settingsNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Settings);
        m_settingsPage->refresh();
    });
}

void MainWindow::showPage(Page page) const
{
    m_stack->setCurrentIndex(static_cast<int>(page));

    m_logoBtn->setChecked(page == Page::Vpn);
    m_countriesNavBtn->setChecked(page == Page::Countries);
    m_accountNavBtn->setChecked(page == Page::Account);
    m_settingsNavBtn->setChecked(page == Page::Settings);
}

void MainWindow::setNavActive(QToolButton* btn)
{
    for (auto* b : {m_logoBtn, m_countriesNavBtn, m_accountNavBtn, m_settingsNavBtn})
        b->setChecked(b == btn);
}

void MainWindow::startupCheck() const
{
    m_manager->checkInstalled();
}

void MainWindow::sendNotification(const QString& title, const QString& message) const
{
    if (!AppConfig::instance().notifications())
        return;

    // Render the ProtonVPN sign SVG into a pixmap to use as the notification icon.
    QPixmap iconPix(64, 64);
    iconPix.fill(Qt::transparent);
    QPainter p(&iconPix);
    QSvgRenderer renderer(QStringLiteral(":/assets/proton-vpn-sign.svg"));
    renderer.render(&p);
    p.end();

    m_trayIcon->showMessage(title, message, QIcon(iconPix), 4000 /*ms*/);
}

void MainWindow::updateTrayIcon(VpnState state)
{
    // Choose asset based on state
    QString asset;
    switch (state)
    {
    case VpnState::Connected:
        asset = QStringLiteral(":/assets/state-connected.svg");
        break;
    case VpnState::Connecting:
    case VpnState::Disconnecting:
        asset = QStringLiteral(":/assets/state-connecting.svg");
        break;
    case VpnState::Error:
        asset = QStringLiteral(":/assets/state-error.svg");
        break;
    default:
        asset = QStringLiteral(":/assets/state-disconnected.svg");
        break;
    }

    // Render into a pixmap (use a larger size for the window icon, smaller for tray)
    auto makeIcon = [&](const int sz)
    {
        QPixmap pix(sz, sz);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        QSvgRenderer renderer(asset);
        renderer.render(&p);
        return QIcon(pix);
    };

    m_trayIcon->setIcon(makeIcon(22));

    switch (state)
    {
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
    default: // Unknown — still checking
        m_trayIcon->setToolTip(QStringLiteral("ProtonVPN – Checking…"));
        m_trayConnectAction->setText(QStringLiteral("Connect"));
        m_trayConnectAction->setEnabled(false);
        break;
    }

    // Send a desktop notification on meaningful state transitions (avoid re-notifying same state)
    if (state != m_lastNotifiedState)
    {
        switch (state)
        {
        case VpnState::Connecting:
            sendNotification(QStringLiteral("ProtonVPN – Connecting"),
                             QStringLiteral("Establishing a secure VPN connection…"));
            break;
        case VpnState::Disconnecting:
            sendNotification(QStringLiteral("ProtonVPN – Disconnecting"),
                             QStringLiteral("Closing the VPN connection…"));
            break;
        case VpnState::Connected:
            sendNotification(QStringLiteral("ProtonVPN – Connected"),
                             QStringLiteral("You are now protected by ProtonVPN."));
            break;
        case VpnState::Disconnected:
            // Only notify on disconnect if we were previously connected/connecting
            if (m_lastNotifiedState == VpnState::Connected ||
                m_lastNotifiedState == VpnState::Disconnecting)
            {
                sendNotification(QStringLiteral("ProtonVPN – Disconnected"),
                                 QStringLiteral("The VPN connection has been closed."));
            }
            break;
        default:
            break;
        }
        m_lastNotifiedState = state;
    }
}

