#include <QApplication>
#include <QButtonGroup>
#include <QFile>
#include <QFrame>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QSvgWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVersionNumber>
#include <utility>
#include "appConfig.h"
#include "connectionHistory.h"
#include "debug.h"
#include "dialogs/updateAvailableDialog.h"
#include "dialogs/whatsNewDialog.h"
#include "geoUtils.h"
#include "mainWindow.h"
#include "pages/accountPage.h"
#include "pages/countriesPage.h"
#include "pages/loginPage.h"
#include "pages/notInstalledPage.h"
#include "pages/settingsPage.h"
#include "pages/vpnPage.h"
#ifdef QT_DEBUG
#include "pages/debugPage.h"
#endif

namespace
{
// Window dimensions
constexpr int WINDOW_ICON_SIZE       = 64;
constexpr int MIN_WINDOW_WIDTH       = 460;
constexpr int MIN_WINDOW_HEIGHT      = 580;
constexpr int INITIAL_WINDOW_WIDTH   = 660;
constexpr int INITIAL_WINDOW_HEIGHT  = 600;

// Sidebar layout
constexpr int SIDEBAR_WIDTH          = 64;
constexpr int SIDEBAR_DIVIDER_WIDTH  = 1;
constexpr int SIDEBAR_MARGIN         = 8;
constexpr int SIDEBAR_LAYOUT_SPACING = 4;
constexpr int SIDEBAR_LOGO_SPACING   = 12;

// Button sizes
constexpr int LOGO_BTN_ICON_SIZE     = 40;
constexpr int LOGO_BTN_SIZE          = 56;
constexpr int NAV_ICON_SIZE          = 24;
constexpr int NAV_BTN_SIZE           = 48;

// Theme tint detection: lightness() returns 0–255; below midpoint = dark palette
constexpr int DARK_THEME_LIGHTNESS_THRESHOLD = 128;

// Proton dark-theme nav tint color (#1a1a2e)
constexpr int DARK_BG_R = 0x1a;
constexpr int DARK_BG_G = 0x1a;
constexpr int DARK_BG_B = 0x2e;

// Quit dialog
constexpr int QUIT_DIALOG_MIN_WIDTH         = 440;
constexpr int QUIT_DIALOG_SPACING           = 16;
constexpr int QUIT_DIALOG_MARGIN            = 24;
constexpr int QUIT_DIALOG_BOTTOM_MARGIN     = 20;
constexpr int QUIT_DIALOG_BTN_ROW_SPACING   = 8;
constexpr int QUIT_DIALOG_RESULT_DISCONNECT = 2;

// Notifications and tray
constexpr int NOTIFICATION_ICON_SIZE   = 64;
constexpr int NOTIFICATION_DURATION_MS = 4000;
constexpr int TRAY_ICON_SIZE           = 22;

// Update check
constexpr int UPDATE_CHECK_DELAY_MS   = 3000;
constexpr int UPDATE_CHECK_TIMEOUT_MS = 10000;
constexpr const char* UPDATE_VERSION_URL =
    "https://raw.githubusercontent.com/wheat32/proton-vpn-qt-app/main/src/version.json";

// Startup
constexpr int WHATS_NEW_DELAY_MS = 400;

// Renders the SVG at `path` into a QIcon of `size`.
// When `tintForTheme` is true (used for monochrome utility icons), the result
// is tinted white on dark backgrounds and dark navy on light backgrounds so
// the icon is always legible.  Pass false for branded/colored logos that
// should be rendered with their own SVG colors unchanged.
QIcon svgNavIcon(const QString& path, const QSize& size = {NAV_ICON_SIZE, NAV_ICON_SIZE}, bool tintForTheme = true)
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);

    if (tintForTheme)
    {
        const QColor windowColor = QApplication::palette().color(QPalette::Window);
        const QColor tintColor = (windowColor.lightness() < DARK_THEME_LIGHTNESS_THRESHOLD)
                                     ? Qt::white
                                     : QColor(DARK_BG_R, DARK_BG_G, DARK_BG_B);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pix.rect(), tintColor);
    }

    p.end();
    return QIcon(pix);
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("ProtonVPN"));
    setWindowIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {WINDOW_ICON_SIZE, WINDOW_ICON_SIZE}, false));
    setMinimumSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    resize(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);

    m_manager = new VpnManager(this);
    m_networkManager = new QNetworkAccessManager(this);

    // Root layout: sidebar + content
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Sidebar
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setFixedWidth(SIDEBAR_WIDTH);
    setupSidebar();
    m_sidebar->setEnabled(false); // disabled until startup checks complete
    rootLayout->addWidget(m_sidebar);

    // Vertical divider
    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setFixedWidth(SIDEBAR_DIVIDER_WIDTH);
    divider->setObjectName(QStringLiteral("sidebarDivider"));
    rootLayout->addWidget(divider);

    // Stacked content
    m_stack = new QStackedWidget(this);
    rootLayout->addWidget(m_stack);

    // Loading page (index 0)
    auto* loadingPage = new QWidget();
    auto* loadingLayout = new QVBoxLayout(loadingPage);
    loadingLayout->setAlignment(Qt::AlignCenter);
    auto* loadingLabel = new QLabel(tr("Starting\u2026"), loadingPage);
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
        m_loginUsername = u;
        m_loginPassword = p;
        m_pending2FAToken.clear();
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
        m_manager->login(u, p);
    });
    connect(m_loginPage, &LoginPage::twoFASubmitted, this, [this](const QString& token)
    {
        m_loginPage->setLoading(true);
        m_loginPage->setError(QString());
        if (m_manager->isLoginInProgress())
        {
            m_manager->submit2FA(token);
        }
        else
        {
            // The signin process died after a failed 2FA attempt. Restart login
            // with the saved credentials and auto-submit the new token once the
            // password prompt is cleared and the 2FA prompt appears again.
            m_pending2FAToken = token;
            m_manager->login(m_loginUsername, m_loginPassword);
        }
    });
    connect(m_loginPage, &LoginPage::loginCancelRequested, this, [this]()
    {
        m_manager->cancelLogin();
        m_loginUsername.clear();
        m_loginPassword.clear();
        m_pending2FAToken.clear();
        m_loginPage->reset();
    });

    // VPN page (index 3)
    m_vpnPage = new VpnPage(m_manager);
    m_stack->addWidget(m_vpnPage); // index 3
    connect(m_vpnPage, &VpnPage::connectRequested, m_manager,
            [this](const QString& country, const QString& city)
            {
                if (country.isEmpty() == false && city.isEmpty() == false)
                {
                    const QString name = GeoUtils::countryCodeToName(country);
                    ConnectionHistory::instance().record(country, name, city);
                }
                m_manager->connectVpn(country, city);
            });
    connect(m_vpnPage, &VpnPage::disconnectRequested, m_manager, &VpnManager::disconnectVpn);
    connect(m_vpnPage, &VpnPage::signOutRequested, this, [this]()
    {
        m_manager->signOut();
    });
    connect(m_vpnPage, &VpnPage::changeCountryRequested, this, [this]()
    {
        showPage(Page::Countries);
    });

    // Countries page (index 4)
    m_countriesPage = new CountriesPage(m_manager);
    m_stack->addWidget(m_countriesPage); // index 4
    connect(m_countriesPage, &CountriesPage::connectRequested, this,
            [this](const QString& country, const QString& city)
            {
                m_vpnPage->notifyExternalConnect(city);
                if (country.isEmpty() == false && city.isEmpty() == false)
                {
                    const QString name = GeoUtils::countryCodeToName(country);
                    ConnectionHistory::instance().record(country, name, city);
                }
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
    m_settingsPage = new SettingsPage(m_manager, m_vpnPage->natPmpManager());
    m_stack->addWidget(m_settingsPage); // index 6
    connect(m_settingsPage, &SettingsPage::recentConnectionsCleared,
            m_vpnPage, &VpnPage::refreshRecentPicker);
    connect(m_settingsPage, &SettingsPage::locationPickerVisibilityChanged,
            m_vpnPage, &VpnPage::setLocationPickerVisible);
    connect(m_settingsPage, &SettingsPage::favoritesDropdownVisibilityChanged,
            m_vpnPage, &VpnPage::setFavoritesDropdownVisible);
    connect(m_settingsPage, &SettingsPage::favoritesEnabledChanged,
            m_vpnPage, &VpnPage::setFavoritesEnabled);
    connect(m_settingsPage, &SettingsPage::favoritesCleared,
            m_vpnPage, &VpnPage::refreshFavoritesPicker);

#ifdef QT_DEBUG
    // Debug page (index 7) – only present in debug builds
    m_debugPage = new DebugPage();
    m_stack->addWidget(m_debugPage); // index 7
#endif

    // Keep the recent picker in sync with any history change (record, clear, trim).
    connect(&ConnectionHistory::instance(), &ConnectionHistory::changed,
            m_vpnPage, &VpnPage::refreshRecentPicker);

    // VpnManager signals
    connect(m_manager, &VpnManager::connectionCityKnown,
            m_vpnPage, &VpnPage::onStatusCityKnown);

    // Show CLI version-mismatch banner on the login page as well as the VPN page.
    connect(m_manager, &VpnManager::cliVersionReady,
            m_loginPage, &LoginPage::onCliVersionReady);

    connect(m_manager, &VpnManager::installedResult, this, [this](bool installed)
    {
        if (installed == false)
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
            if (AppConfig::instance().autoConnect() == true)
            {
                m_startupAutoConnectPending = true;
            }
        }
        else
        {
            m_sidebar->setEnabled(false);
            showPage(Page::Login);
        }
        // Delay slightly so the page transition is visible before the dialog pops.
        QTimer::singleShot(WHATS_NEW_DELAY_MS, this, &MainWindow::maybeShowWhatsNew);
    });

    connect(m_manager, &VpnManager::twoFactorRequired, this, [this]()
    {
        if (m_pending2FAToken.isEmpty() == false)
        {
            // Retry path: automatically submit the token the user already typed
            // without bouncing the UI back to the 2FA input screen.
            m_manager->submit2FA(m_pending2FAToken);
            m_pending2FAToken.clear();
        }
        else
        {
            m_loginPage->setLoading(false);
            m_loginPage->show2FAPrompt();
        }
    });

    connect(m_manager, &VpnManager::loginFinished, this, [this](bool ok, const QString& error)
    {
        m_loginPage->setLoading(false);
        if (ok)
        {
            m_loginUsername.clear();
            m_loginPassword.clear();
            m_pending2FAToken.clear();
            m_loginPage->reset();
            m_sidebar->setEnabled(true);
            showPage(Page::Vpn);
            m_manager->fetchCountries();
        }
        else
        {
            m_loginPage->setError(error.isEmpty()
                                      ? tr("Login failed. Please check your credentials.")
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

                // If the CLI reports that the session has expired / is not
                // authenticated, sign the user out automatically so they land
                // back on the login page rather than being stuck on an error.
                if (state == VpnState::Error)
                {
                    const QString lower = info.toLower();
                    const bool isAuthError =
                        lower.contains(QLatin1String("authentication required")) ||
                        lower.contains(QLatin1String("please sign in with"))     ||
                        lower.contains(QLatin1String("401"));
                    if (isAuthError)
                    {
                        m_manager->signOut();
                        return;
                    }
                }

                // Auto-connect: if we flagged a pending connect and we just confirmed
                // the VPN is disconnected, initiate the connection now.
                if (m_startupAutoConnectPending && state == VpnState::Disconnected)
                {
                    m_startupAutoConnectPending = false;
                    const QString serverKey = AppConfig::instance().autoConnectServer();
                    if (serverKey.isEmpty())
                    {
                        m_manager->connectVpn();
                    }
                    else
                    {
                        const int sep = serverKey.indexOf(QLatin1Char('|'));
                        const QString country = (sep >= 0) ? serverKey.left(sep) : serverKey;
                        const QString city    = (sep >= 0) ? serverKey.mid(sep + 1) : QString();
                        m_manager->connectVpn(country, city);
                    }
                }
            });

    // System tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    auto* trayMenu = new QMenu(this);
    trayMenu->addAction(tr("Show"), this, [this]()
    {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addSeparator();
    m_trayConnectAction = trayMenu->addAction(tr("Connect"), this, [this]()
    {
        const VpnState state = m_manager->currentState();
        if (state == VpnState::Connected)
        {
            m_manager->disconnectVpn();
        }
        else if (state == VpnState::Disconnected || state == VpnState::Error)
        {
            m_manager->connectVpn();
        }
    });
    trayMenu->addSeparator();
    trayMenu->addAction(tr("Quit"), this, [this]()
    {
        // Only prompt when the VPN is active
        if (m_manager->currentState() != VpnState::Connected &&
            m_manager->currentState() != VpnState::Connecting)
        {
            QApplication::quit();
            return;
        }

        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(tr("Quit ProtonVPN"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setModal(true);
        dlg->setMinimumWidth(QUIT_DIALOG_MIN_WIDTH);

        auto* layout = new QVBoxLayout(dlg);
        layout->setSpacing(QUIT_DIALOG_SPACING);
        layout->setContentsMargins(QUIT_DIALOG_MARGIN, QUIT_DIALOG_MARGIN, QUIT_DIALOG_MARGIN, QUIT_DIALOG_BOTTOM_MARGIN);

        auto* msgLabel = new QLabel(
            QStringLiteral("%1<br>%2").arg(
                tr("The VPN is currently active.").toHtmlEscaped(),
                tr("What would you like to do before quitting?").toHtmlEscaped()),
            dlg);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        layout->addWidget(msgLabel);

        if (m_vpnPage->isPortForwardingActive())
        {
            auto* pfLabel = new QLabel(
                QStringLiteral("<i>%1</i>").arg(
                    tr("Note: the forwarded port lease will lapse shortly after "
                       "the app closes, as the keep-alive loop will no longer be running.")
                    .toHtmlEscaped()),
                dlg);
            pfLabel->setWordWrap(true);
            pfLabel->setTextFormat(Qt::RichText);
            layout->addWidget(pfLabel);
        }

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(QUIT_DIALOG_BTN_ROW_SPACING);

        auto* cancelBtn = new QPushButton(tr("Cancel"), dlg);
        cancelBtn->setObjectName(QStringLiteral("secondaryButton"));

        auto* leaveOnBtn = new QPushButton(tr("Leave VPN on"), dlg);
        leaveOnBtn->setObjectName(QStringLiteral("leaveVpnOnButton"));
        leaveOnBtn->setDefault(true);

        auto* disconnectBtn = new QPushButton(tr("Disconnect VPN"), dlg);
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
            dlg->done(QUIT_DIALOG_RESULT_DISCONNECT); // custom result code for "disconnect then quit"
        });

        const int result = dlg->exec();
        if (result == QDialog::Rejected)
            return; // user canceled - do nothing

        if (result == QUIT_DIALOG_RESULT_DISCONNECT)
        {
            m_manager->disconnectVpnSync(); // blocks until protonvpn disconnect finishes
        }

        QApplication::quit();
    });
    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](const QSystemTrayIcon::ActivationReason reason)
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
    QTimer::singleShot(UPDATE_CHECK_DELAY_MS, this, &MainWindow::checkForUpdates);
}

void MainWindow::setupSidebar()
{
    auto* layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(0, SIDEBAR_MARGIN, 0, SIDEBAR_MARGIN);
    layout->setSpacing(SIDEBAR_LAYOUT_SPACING);
    layout->setAlignment(Qt::AlignHCenter);

    auto* btnGroup = new QButtonGroup(m_sidebar);
    btnGroup->setExclusive(true);

    // Logo button is part of the exclusive group so clicking it automatically
    // clears the checked state on all nav buttons.
    m_logoBtn = new QToolButton(m_sidebar);
    m_logoBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {LOGO_BTN_ICON_SIZE, LOGO_BTN_ICON_SIZE}, false));
    m_logoBtn->setIconSize({LOGO_BTN_ICON_SIZE, LOGO_BTN_ICON_SIZE});
    m_logoBtn->setFixedSize(LOGO_BTN_SIZE, LOGO_BTN_SIZE);
    m_logoBtn->setToolTip(tr("VPN"));
    m_logoBtn->setCursor(Qt::PointingHandCursor);
    m_logoBtn->setObjectName(QStringLiteral("logoButton"));
    m_logoBtn->setCheckable(true);
    btnGroup->addButton(m_logoBtn);
    layout->addWidget(m_logoBtn, 0, Qt::AlignHCenter);
    connect(m_logoBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Vpn); });

    layout->addSpacing(SIDEBAR_LOGO_SPACING);

    auto makeNavBtn = [&](const QString& tooltip, const QString& iconPath) -> QToolButton*
    {
        QToolButton* btn = new QToolButton(m_sidebar);
        btn->setToolTip(tooltip);
        btn->setIcon(svgNavIcon(iconPath, {NAV_ICON_SIZE, NAV_ICON_SIZE}));
        btn->setIconSize({NAV_ICON_SIZE, NAV_ICON_SIZE});
        btn->setFixedSize(NAV_BTN_SIZE, NAV_BTN_SIZE);
        btn->setCheckable(true);
        btn->setObjectName(QStringLiteral("navButton"));
        btn->setCursor(Qt::PointingHandCursor);
        btnGroup->addButton(btn);
        layout->addWidget(btn, 0, Qt::AlignHCenter);
        return btn;
    };

    m_countriesNavBtn = makeNavBtn(tr("Countries"), QStringLiteral(":/assets/server-smart-routing.svg"));
    m_accountNavBtn = makeNavBtn(tr("Account"), QStringLiteral(":/assets/person-lines-fill.svg"));

    connect(m_countriesNavBtn, &QToolButton::clicked, this, [this]() { showPage(Page::Countries); });
    connect(m_accountNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Account);
        m_accountPage->refresh();
    });

    layout->addStretch();

    // Settings button pinned to the bottom of the sidebar
    m_settingsNavBtn = makeNavBtn(tr("Settings"), QStringLiteral(":/assets/gear.svg"));
    connect(m_settingsNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Settings);
        m_settingsPage->refresh();
    });

#ifdef QT_DEBUG
    // Debug button – visible by default in debug builds, toggled with F11
    m_debugNavBtn = makeNavBtn(tr("Debug"), QStringLiteral(":/assets/bug.svg"));
    connect(m_debugNavBtn, &QToolButton::clicked, this, [this]()
    {
        showPage(Page::Debug);
    });
#endif
}

void MainWindow::showPage(Page page) const
{
    m_stack->setCurrentIndex(std::to_underlying(page));

    m_logoBtn->setChecked(page == Page::Vpn);
    m_countriesNavBtn->setChecked(page == Page::Countries);
    m_accountNavBtn->setChecked(page == Page::Account);
    m_settingsNavBtn->setChecked(page == Page::Settings);
#ifdef QT_DEBUG
    if (m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setChecked(page == Page::Debug);
    }
#endif
}

void MainWindow::setNavActive(const QToolButton* btn)
{
    for (QToolButton* b : {m_logoBtn, m_countriesNavBtn, m_accountNavBtn, m_settingsNavBtn})
    {
        b->setChecked(b == btn);
    }
#ifdef QT_DEBUG
    if (m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setChecked(m_debugNavBtn == btn);
    }
#endif
}

void MainWindow::startupCheck() const
{
    m_manager->checkInstalled();
}

void MainWindow::checkForUpdates()
{
    if (AppConfig::instance().checkForUpdates() == false)
        return;

    DBG_APP(QStringLiteral("Checking for updates..."));
    QNetworkRequest request(QUrl(QString::fromLatin1(UPDATE_VERSION_URL)));
    request.setTransferTimeout(UPDATE_CHECK_TIMEOUT_MS);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            DBG_APP(QStringLiteral("Update check failed: ") + reply->errorString());
            return;
        }

        const QJsonObject remoteObj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString remoteVersion = remoteObj.value(QStringLiteral("app_version")).toString();
        if (remoteVersion.isEmpty())
            return;

        QString localVersion;
        QFile vf(QStringLiteral(":/version.json"));
        if (vf.open(QIODevice::ReadOnly))
        {
            localVersion = QJsonDocument::fromJson(vf.readAll())
                               .object().value(QStringLiteral("app_version")).toString();
        }
        if (localVersion.isEmpty())
            return;

        const QVersionNumber remote = QVersionNumber::fromString(remoteVersion);
        const QVersionNumber local  = QVersionNumber::fromString(localVersion);

        if (remote > local)
        {
            DBG_APP(QStringLiteral("Update available: v") + localVersion
                    + QStringLiteral(" → v") + remoteVersion);
            UpdateAvailableDialog* dlg = new UpdateAvailableDialog(localVersion, remoteVersion, this);
            dlg->setModal(true);
            dlg->show();
        }
        else
        {
            DBG_APP(QStringLiteral("Up to date (v") + localVersion + QStringLiteral(")"));
        }
    });
}

void MainWindow::refreshIcons()
{
    // Logo button: render with original SVG colors (no tinting) - it's a branded icon.
    setWindowIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {WINDOW_ICON_SIZE, WINDOW_ICON_SIZE}, false));
    m_logoBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/proton-vpn-sign.svg"), {LOGO_BTN_ICON_SIZE, LOGO_BTN_ICON_SIZE}, false));
    // Nav icons: monochrome utility icons - tint for legibility.
    m_countriesNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/server-smart-routing.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    m_accountNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/person-lines-fill.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    m_settingsNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/gear.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
#ifdef QT_DEBUG
    if (m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setIcon(svgNavIcon(QStringLiteral(":/assets/bug.svg"), {NAV_ICON_SIZE, NAV_ICON_SIZE}));
    }
#endif
}

void MainWindow::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        refreshIcons();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
#ifdef QT_DEBUG
    if (event->key() == Qt::Key_F11 && m_debugNavBtn != nullptr)
    {
        m_debugNavBtn->setVisible(m_debugNavBtn->isVisible() == false);
        // If the debug page is currently shown and we just hid the button,
        // navigate away to avoid being stuck on a visually orphaned page.
        if (m_debugNavBtn->isVisible() == false &&
            m_stack->currentIndex() == std::to_underlying(Page::Debug))
        {
            showPage(Page::Vpn);
        }
        event->accept();
        return;
    }
#endif
    QWidget::keyPressEvent(event);
}

void MainWindow::sendNotification(const QString& title, const QString& message) const
{
    if (AppConfig::instance().notifications() == false)
        return;

    // Render the ProtonVPN sign SVG into a pixmap to use as the notification icon.
    QPixmap iconPix(NOTIFICATION_ICON_SIZE, NOTIFICATION_ICON_SIZE);
    iconPix.fill(Qt::transparent);
    QPainter p(&iconPix);
    QSvgRenderer renderer(QStringLiteral(":/assets/proton-vpn-sign.svg"));
    renderer.render(&p);
    p.end();

    m_trayIcon->showMessage(title, message, QIcon(iconPix), NOTIFICATION_DURATION_MS);
}

void MainWindow::updateTrayIcon(VpnState state)
{    // Choose asset based on state
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

    m_trayIcon->setIcon(makeIcon(TRAY_ICON_SIZE));

    switch (state)
    {
    case VpnState::Connected:
        m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Connected"));
        m_trayConnectAction->setText(tr("Disconnect"));
        m_trayConnectAction->setEnabled(true);
        break;
    case VpnState::Connecting:
        m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Connecting\u2026"));
        m_trayConnectAction->setText(tr("Connecting\u2026"));
        m_trayConnectAction->setEnabled(false);
        break;
    case VpnState::Disconnecting:
        m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Disconnecting\u2026"));
        m_trayConnectAction->setText(tr("Disconnecting\u2026"));
        m_trayConnectAction->setEnabled(false);
        break;
    case VpnState::Error:
        m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Error"));
        m_trayConnectAction->setText(tr("Connect"));
        m_trayConnectAction->setEnabled(true);
        break;
    case VpnState::Disconnected:
        m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Disconnected"));
        m_trayConnectAction->setText(tr("Connect"));
        m_trayConnectAction->setEnabled(true);
        break;
    default: // Unknown - still checking
        m_trayIcon->setToolTip(tr("ProtonVPN \u2013 Checking\u2026"));
        m_trayConnectAction->setText(tr("Connect"));
        m_trayConnectAction->setEnabled(false);
        break;
    }

    // Send a desktop notification on meaningful state transitions (avoid re-notifying same state)
    if (state != m_lastNotifiedState)
    {
        switch (state)
        {
        case VpnState::Connecting:
            sendNotification(tr("ProtonVPN \u2013 Connecting"),
                             tr("Establishing a secure VPN connection\u2026"));
            break;
        case VpnState::Disconnecting:
            sendNotification(tr("ProtonVPN \u2013 Disconnecting"),
                             tr("Closing the VPN connection\u2026"));
            break;
        case VpnState::Connected:
            sendNotification(tr("ProtonVPN \u2013 Connected"),
                             tr("You are now protected by ProtonVPN."));
            break;
        case VpnState::Disconnected:
            // Only notify on disconnect if we were previously connected/connecting
            if (m_lastNotifiedState == VpnState::Connected ||
                m_lastNotifiedState == VpnState::Disconnecting)
            {
                sendNotification(tr("ProtonVPN \u2013 Disconnected"),
                                 tr("The VPN connection has been closed."));
            }
            break;
        default:
            break;
        }
        m_lastNotifiedState = state;
    }
}

void MainWindow::maybeShowWhatsNew()
{
    if (m_whatsNewShown)
        return;

    m_whatsNewShown = true;

    // Read the current app version from the embedded version.json resource.
    QString currentVersion;
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        currentVersion = obj.value(QStringLiteral("app_version")).toString();
    }

    if (currentVersion.isEmpty())
        return;

    const QString lastSeen = AppConfig::instance().lastSeenVersion();

    // Show the dialog if this is the first launch or a version change was detected.
    if (lastSeen != currentVersion)
    {
        // Update the stored version immediately so repeated crashes don't keep
        // showing the dialog on every launch.
        AppConfig::instance().setLastSeenVersion(currentVersion);

        auto* dlg = new WhatsNewDialog(currentVersion, this);
        dlg->setModal(true);
        dlg->show();
    }
}
