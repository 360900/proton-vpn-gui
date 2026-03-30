#include "settingspage.h"
#include "../appconfig.h"
#include "../connectionhistory.h"
#include "../uihelpers.h"

#include <QSpinBox>
#include <QVBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include "../widgets/numberspinner.h"
#include "../widgets/toastnotification.h"
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QDialogButtonBox> // ignore unused include warning for QDialogButtonBox
#include <QTextBrowser>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QDebug>

// ============================================================
// ToggleSwitch
// ============================================================

ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent)
{
    setFixedSize(ToggleSwitch::sizeHint());
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);
}

void ToggleSwitch::setOn(const bool on, const bool animate)
{
    if (m_on == on) return;
    m_on = on;
    if (animate)
    {
        m_anim->stop();
        m_anim->setStartValue(m_knobPos);
        m_anim->setEndValue(on ? 1.0 : 0.0);
        m_anim->start();
    }
    else
    {
        m_knobPos = on ? 1.0 : 0.0;
        update();
    }
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int w = width(), h = height(), r = h / 2;
    constexpr QColor trackOn(0x6d, 0x4a, 0xff), trackOff(0x55, 0x55, 0x66);
    QColor track;
    track.setRed(static_cast<int>(trackOff.red() + (trackOn.red() - trackOff.red()) * m_knobPos));
    track.setGreen(static_cast<int>(trackOff.green() + (trackOn.green() - trackOff.green()) * m_knobPos));
    track.setBlue(static_cast<int>(trackOff.blue() + (trackOn.blue() - trackOff.blue()) * m_knobPos));
    p.setBrush(track);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, w, h, r, r);
    const int knobD = h - 4, knobMin = 2, knobMax = w - knobD - 2;
    const int knobX = static_cast<int>(knobMin + (knobMax - knobMin) * m_knobPos);
    p.setBrush(Qt::white);
    p.drawEllipse(knobX, 2, knobD, knobD);
}

void ToggleSwitch::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        setOn(!m_on);
        emit toggled(m_on);
    }
    QWidget::mousePressEvent(e);
}

// ============================================================
// SettingsPage helpers
// ============================================================

void SettingsPage::addDivider(QVBoxLayout* layout, QWidget* parent)
{
    auto* div = new QFrame(parent);
    div->setFrameShape(QFrame::HLine);
    div->setObjectName(QStringLiteral("divider"));
    layout->addWidget(div);
}

static QWidget* makeTextCol(QWidget* parent, const QString& label, const QString& desc)
{
    auto* w = new QWidget(parent);
    w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* col = new QVBoxLayout(w);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(2);
    auto* nameL = new QLabel(label, w);
    nameL->setObjectName(QStringLiteral("infoKey"));
    col->addWidget(nameL);
    if (!desc.isEmpty())
    {
        auto* descL = new QLabel(desc, w);
        descL->setObjectName(QStringLiteral("settingsDesc"));
        descL->setWordWrap(true);
        descL->setOpenExternalLinks(true);
        descL->setTextInteractionFlags(Qt::TextBrowserInteraction);
        QFont f = descL->font();
        f.setPointSize(qMax(f.pointSize() - 1, 7));
        descL->setFont(f);
        descL->setStyleSheet(QStringLiteral("color: #888;"));
        col->addWidget(descL);
    }
    return w;
}

void SettingsPage::maybeWarnReconnect(const QString& cliOutput)
{
    // The CLI emits phrases like "please establish a new VPN connection for
    // changes to take effect" when a reconnect is required.
    // Only warn when we are actually connected so the message is relevant.
    const bool needsReconnect =
        cliOutput.contains(QStringLiteral("new VPN connection"), Qt::CaseInsensitive) ||
        cliOutput.contains(QStringLiteral("establish a new"), Qt::CaseInsensitive);

    if (!needsReconnect) return;
    if (m_manager->currentState() != VpnState::Connected) return;

    QMessageBox mb(this);
    mb.setWindowTitle(QStringLiteral("Reconnect Required"));
    mb.setIcon(QMessageBox::Information);
    mb.setText(QStringLiteral("This setting change will only take effect after reconnecting to the VPN."));
    mb.setStandardButtons(QMessageBox::Ok);
    mb.exec();
}

QWidget* SettingsPage::makeToggleRow(QWidget* parent, const QString& label,
                                     const QString& desc, const QString& cliKey)
{
    auto* row = new QWidget(parent);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(16, 12, 16, 12);
    rl->setSpacing(16);
    rl->addWidget(makeTextCol(row, label, desc), 1);
    auto* toggle = new ToggleSwitch(row);
    rl->addWidget(toggle, 0);

    connect(toggle, &ToggleSwitch::toggled, this, [this, cliKey](bool on)
    {
        m_manager->applyConfigValue(cliKey, on ? QStringLiteral("on") : QStringLiteral("off"));
        // Reconnect detection is handled via the configApplied signal.
    });

    m_toggleRows.append({cliKey, toggle});
    return row;
}

QWidget* SettingsPage::makeComboRow(QWidget* parent, const QString& label,
                                    const QString& desc, const QString& cliKey,
                                    const QStringList& labels, const QStringList& cliValues)
{
    auto* row = new QWidget(parent);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(16, 12, 16, 12);
    rl->setSpacing(16);
    rl->addWidget(makeTextCol(row, label, desc), 1);
    auto* combo = new QComboBox(row);
    for (const auto& l : labels) combo->addItem(l);
    combo->setMinimumWidth(160);
    rl->addWidget(combo, 0);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, cliKey, cliValues](int idx)
            {
                if (idx < 0 || idx >= cliValues.size()) return;
                m_manager->applyConfigValue(cliKey, cliValues[idx]);
                // Reconnect detection is handled via the configApplied signal.
            });

    m_comboRows.append({cliKey, combo, cliValues});
    return row;
}

// ============================================================
// Auto-start helpers (systemd user service)
// ============================================================

// Easy-to-change constant: directory where the .service file lives.
static const QString kSystemdUserServiceDir =
    QDir::homePath() + QStringLiteral("/.config/systemd/user");
static const QString kServiceName = QStringLiteral("proton-vpn-qt.service");

// In debug builds, skip actual systemd/file operations so the toggle can be
// tested without installing a real service. Flip to false to test for real.
#ifdef QT_DEBUG
static constexpr bool kDryRun = true;
#else
static constexpr bool kDryRun = false;
#endif

QString SettingsPage::serviceFilePath()
{
    return kSystemdUserServiceDir + QLatin1Char('/') + kServiceName;
}

bool SettingsPage::systemdAvailable()
{
    // Check once whether systemctl is on PATH by trying to start it with --version.
    static int cached = -1; // -1 = unchecked, 0 = absent, 1 = present
    if (cached != -1) return cached == 1;

    QProcess p;
    p.start(QStringLiteral("systemctl"), {QStringLiteral("--version")});
    cached = (p.waitForStarted(2000) && p.waitForFinished(2000)) ? 1 : 0;
    return cached == 1;
}

bool SettingsPage::autoStartEnabled()
{
    if (kDryRun) return false; // dry-run: always report disabled
    if (!systemdAvailable()) return false;

    // Ask systemd whether the service is enabled. Exit code 0 = enabled.
    QProcess p;
    p.start(QStringLiteral("systemctl"),
            {QStringLiteral("--user"), QStringLiteral("is-enabled"), kServiceName});
    if (!p.waitForFinished(3000)) return false;
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed().toLower();
    return out == QStringLiteral("enabled") || out == QStringLiteral("static");
}

bool SettingsPage::setAutoStart(const bool enable, QString& errorOut)
{
    if (kDryRun)
    {
        qDebug("[DryRun] setAutoStart(%s) — skipping real systemd operations.",
               enable ? "true" : "false");
        return true; // pretend success
    }

    if (enable)
    {
        // Resolve the path to the currently running executable.
        const QString exe = QCoreApplication::applicationFilePath();

        // Create the systemd user service directory if it doesn't exist.
        QDir dir;
        if (!dir.mkpath(kSystemdUserServiceDir))
        {
            errorOut = QStringLiteral("Could not create directory: ") + kSystemdUserServiceDir;
            return false;
        }

        // Write the .service file.
        QFile templateFile(QStringLiteral(":/init/systemd/proton-vpn-qt.service"));
        if (!templateFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            errorOut = QStringLiteral("Could not read service template from resources.");
            return false;
        }
        const QString serviceContent = QString::fromUtf8(templateFile.readAll()).arg(exe);
        templateFile.close();

        QFile f(serviceFilePath());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            errorOut = QStringLiteral("Could not write service file: ") + serviceFilePath();
            return false;
        }
        f.write(serviceContent.toUtf8());
        f.close();

        // Enable the service (creates the symlink so it starts automatically).
        QProcess p;
        p.start(QStringLiteral("systemctl"),
                {QStringLiteral("--user"), QStringLiteral("enable"), kServiceName});
        p.waitForFinished(5000);
        if (p.exitCode() != 0)
        {
            errorOut = QString::fromUtf8(p.readAllStandardError()).trimmed();
            if (errorOut.isEmpty())
                errorOut = QStringLiteral("systemctl --user enable failed (exit %1)").arg(p.exitCode());
            return false;
        }
    }
    else
    {
        // Disable and remove.
        QProcess p;
        p.start(QStringLiteral("systemctl"),
                {QStringLiteral("--user"), QStringLiteral("disable"), kServiceName});
        p.waitForFinished(5000);

        QFile::remove(serviceFilePath());
    }
    return true;
}

// ============================================================
// SettingsPage constructor
// ============================================================

void SettingsPage::updateAutoConnectRowVisibility() const
{
    if (!m_autoConnectRow) return;
    const bool show = m_autoStartToggle && m_autoStartToggle->isOn();
    m_autoConnectRow->setVisible(show);
    // If auto-start is turned off, also disable auto-connect and persist that.
    if (!show && m_autoConnectToggle && m_autoConnectToggle->isOn())
    {
        m_autoConnectToggle->setOn(false, false);
        AppConfig::instance().setAutoConnect(false);
    }
}

SettingsPage::SettingsPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 16, 16, 16);
    outerLayout->setSpacing(12);

    // Title
    auto* titleLabel = new QLabel(QStringLiteral("Settings"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    outerLayout->addWidget(titleLabel);

    // ── Tab widget ────────────────────────────────────────────
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("settingsTabs"));
    outerLayout->addWidget(tabs, 1);

    // Helper: build a scrollable card inside a tab page and return its QVBoxLayout.
    // The bool pointer `firstOut` tracks divider insertion for that card.
    auto makeCard = [&](QWidget* tabPage) -> std::pair<QWidget*, QVBoxLayout*>
    {
        auto* scroll = new QScrollArea(tabPage);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* pageLayout = new QVBoxLayout(tabPage);
        pageLayout->setContentsMargins(0, 8, 0, 0);
        pageLayout->setSpacing(8);
        pageLayout->addWidget(scroll, 1);

        auto* card = new QWidget();
        card->setObjectName(QStringLiteral("infoCard"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(0, 0, 0, 0);
        cardLayout->setSpacing(0);
        scroll->setWidget(card);
        return {card, cardLayout};
    };

    // ============================================================
    // TAB 1 – App
    // ============================================================
    auto* appTab = new QWidget();
    auto [appCard, appCardLayout] = makeCard(appTab);
    tabs->addTab(appTab, QStringLiteral("App"));

    bool appFirst = true;
    auto addApp = [&](QWidget* w)
    {
        if (!appFirst) addDivider(appCardLayout, appCard);
        appFirst = false;
        appCardLayout->addWidget(w);
    };

    // ── Launch on Startup ─────────────────────────────────────
    if (systemdAvailable())
    {
        m_autoStartRow = new QWidget(appCard);
        auto* rl = new QHBoxLayout(m_autoStartRow);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(m_autoStartRow,
                                  QStringLiteral("Launch on Startup"),
                                  QStringLiteral("Automatically start the app in the background when you log in "
                                      "(installs a systemd user service).")), 1);
        m_autoStartToggle = new ToggleSwitch(m_autoStartRow);
        m_autoStartToggle->setOn(autoStartEnabled(), false);
        connect(m_autoStartToggle, &ToggleSwitch::toggled, this, [this](bool on)
        {
            QString err;
            if (!setAutoStart(on, err))
            {
                m_autoStartToggle->setOn(!on, false);
                QMessageBox::warning(this,
                                     QStringLiteral("Auto-start Error"),
                                     QStringLiteral("Failed to %1 auto-start:\n%2")
                                     .arg(on ? QStringLiteral("enable") : QStringLiteral("disable"), err));
            }
            else
            {
                updateAutoConnectRowVisibility();
            }
        });
        rl->addWidget(m_autoStartToggle);
        addApp(m_autoStartRow);

        // ── Auto-connect on startup (indented, only when auto-start is on) ──
        m_autoConnectRow = new QWidget(appCard);
        auto* acRl = new QHBoxLayout(m_autoConnectRow);
        acRl->setContentsMargins(32, 8, 16, 12);
        acRl->setSpacing(16);
        acRl->addWidget(makeTextCol(m_autoConnectRow,
                                    QStringLiteral("Auto-connect on Startup"),
                                    QStringLiteral("Automatically connect to the VPN when the app starts.")), 1);
        m_autoConnectToggle = new ToggleSwitch(m_autoConnectRow);
        m_autoConnectToggle->setOn(AppConfig::instance().autoConnect(), false);
        connect(m_autoConnectToggle, &ToggleSwitch::toggled, this, [](bool on)
        {
            AppConfig::instance().setAutoConnect(on);
        });
        acRl->addWidget(m_autoConnectToggle);
        // Added directly so it shares a visual group with autostart (no extra divider above it)
        appCardLayout->addWidget(m_autoConnectRow);

        updateAutoConnectRowVisibility();
    }

    // ── Desktop Notifications ─────────────────────────────────
    {
        auto* row = new QWidget(appCard);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(row,
                                  QStringLiteral("Desktop Notifications"),
                                  QStringLiteral("Show a system notification when the VPN is connecting, "
                                      "connected, disconnecting, or disconnected.")), 1);
        m_notificationsToggle = new ToggleSwitch(row);
        m_notificationsToggle->setOn(AppConfig::instance().notifications(), false);
        connect(m_notificationsToggle, &ToggleSwitch::toggled, this, [](bool on)
        {
            AppConfig::instance().setNotifications(on);
        });
        rl->addWidget(m_notificationsToggle);
        addApp(row);
    }

    // ── Recent Connections ────────────────────────────────────
    {
        auto* row = new QWidget(appCard);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(row,
                                  QStringLiteral("Recent Connections"),
                                  QStringLiteral("Number of recent VPN connections to remember and show "
                                      "on the home screen. Set to 0 to disable.")), 1);
        m_recentConnectionsSpinBox = new NumberSpinner(row);
        m_recentConnectionsSpinBox->setRange(0, 20);
        m_recentConnectionsSpinBox->setValue(AppConfig::instance().recentConnectionsCount());
        connect(m_recentConnectionsSpinBox, &NumberSpinner::valueChanged, this, [](const int val)
        {
            AppConfig::instance().setRecentConnectionsCount(val);
        });
        rl->addWidget(m_recentConnectionsSpinBox);
        addApp(row);
    }

    // ── Clear Recent Connections (only shown when history is non-empty) ────
    {
        // Wrap the divider and the row in one container so hiding the container
        // also hides the divider, leaving no orphaned separator line.
        m_clearRecentRow = new QWidget(appCard);
        auto* cLayout = new QVBoxLayout(m_clearRecentRow);
        cLayout->setContentsMargins(0, 0, 0, 0);
        cLayout->setSpacing(0);

        auto* div = new QFrame(m_clearRecentRow);
        div->setFrameShape(QFrame::HLine);
        div->setObjectName(QStringLiteral("divider"));
        cLayout->addWidget(div);

        auto* inner = new QWidget(m_clearRecentRow);
        auto* rl = new QHBoxLayout(inner);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(inner,
                                  QStringLiteral("Clear Recent Connections"),
                                  QStringLiteral("Remove all saved recent connection history.")), 1);
        auto* clearBtn = new QPushButton(QStringLiteral("Clear"), inner);
        clearBtn->setObjectName(QStringLiteral("dangerButton"));
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, this, [this]()
        {
            ConnectionHistory::instance().clear();
            m_clearRecentRow->setVisible(false);
            emit recentConnectionsCleared();
            ToastNotification::popup(this, QStringLiteral("Recent connection history cleared."));
        });
        rl->addWidget(clearBtn);
        cLayout->addWidget(inner);

        appCardLayout->addWidget(m_clearRecentRow);
        m_clearRecentRow->setVisible(ConnectionHistory::instance().hasAnyEntries());

        connect(&ConnectionHistory::instance(), &ConnectionHistory::changed, this, [this]()
        {
            m_clearRecentRow->setVisible(ConnectionHistory::instance().hasAnyEntries());
        });
    }

    appCardLayout->addStretch();

    // ── About button – sits inside the App tab, below the card ──
    {
        auto* appPageLayout = qobject_cast<QVBoxLayout*>(appTab->layout());
        auto* aboutBtn = new QPushButton(QStringLiteral("About"), appTab);
        aboutBtn->setObjectName(QStringLiteral("secondaryButton"));
        aboutBtn->setCursor(Qt::PointingHandCursor);
        connect(aboutBtn, &QPushButton::clicked, this, &SettingsPage::showAboutDialog);
        appPageLayout->addWidget(aboutBtn);
    }

    // ============================================================
    // TAB 2 – VPN
    // ============================================================
    auto* vpnTab = new QWidget();
    auto [vpnCard, vpnCardLayout] = makeCard(vpnTab);

    // Refresh button + spinner live inside the VPN tab's page layout
    {
        auto* vpnPageLayout = qobject_cast<QVBoxLayout*>(vpnTab->layout());

        auto* headerRow = new QHBoxLayout();
        m_refreshBtn = new QPushButton(QStringLiteral("↻ Refresh"), vpnTab);
        m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
        m_refreshBtn->setFixedHeight(30);
        connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsPage::refresh);
        headerRow->addStretch();
        headerRow->addWidget(m_refreshBtn);
        // Insert the header row above the scroll area (which was inserted as item 0)
        vpnPageLayout->insertLayout(0, headerRow);

        m_statusLabel = new QLabel(vpnTab);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setObjectName(QStringLiteral("settingsStatusLabel"));
        m_statusLabel->setVisible(false);
        vpnPageLayout->insertWidget(1, m_statusLabel);
    }

    tabs->addTab(vpnTab, QStringLiteral("VPN"));

    bool vpnFirst = true;
    auto addVpn = [&](QWidget* w)
    {
        if (!vpnFirst) addDivider(vpnCardLayout, vpnCard);
        vpnFirst = false;
        vpnCardLayout->addWidget(w);
    };

    // ── Anonymous Crash Reports ───────────────────────────────
    addVpn(makeToggleRow(vpnCard,
                         QStringLiteral("Anonymous Crash Reports"),
                         QStringLiteral("Send anonymous crash reports to Proton for the VPN CLI tool — not this Qt app."),
                         QStringLiteral("anonymous-crash-reports")));

    // ── IPv6 ─────────────────────────────────────────────────
    addVpn(makeToggleRow(vpnCard,
                         QStringLiteral("IPv6"),
                         QStringLiteral("Enable IPv6 support over the VPN tunnel."),
                         QStringLiteral("ipv6")));

    // ── NAT Type ──────────────────────────────────────────────
    addVpn(makeComboRow(vpnCard,
                        QStringLiteral("NAT Type"),
                        QStringLiteral(
                            "Controls how the VPN server maps your connection. "
                            "<b>Strict (Type 3)</b> is the default and best for privacy. "
                            "<b>Moderate (Type 2)</b> improves compatibility for online gaming and WebRTC, "
                            "at a slight privacy trade-off. Requires a paid plan. "
                            "<a href='https://protonvpn.com/support/moderate-nat'>Read more</a>"),
                        QStringLiteral("moderate-nat"),
                        {QStringLiteral("Strict (Type 3)"),
                         QStringLiteral("Moderate (Type 2)")},
                        {QStringLiteral("off"), QStringLiteral("on")}));

    // ── Kill Switch ───────────────────────────────────────────
    addVpn(makeComboRow(vpnCard,
                        QStringLiteral("Kill Switch"),
                        QStringLiteral("Block traffic if the VPN connection drops. "
                            "\"Standard\" only blocks while reconnecting; "
                            "\"Permanent\" blocks even when the VPN is off."),
                        QStringLiteral("kill-switch"),
                        {QStringLiteral("Off"), QStringLiteral("Standard"), QStringLiteral("Permanent")},
                        {QStringLiteral("off"), QStringLiteral("standard"), QStringLiteral("full")}));

    // ── VPN Accelerator ───────────────────────────────────────
    addVpn(makeToggleRow(vpnCard,
                         QStringLiteral("VPN Accelerator"),
                         QStringLiteral("Boost connection speeds using advanced protocol techniques."),
                         QStringLiteral("vpn-accelerator")));

    // ── NetShield ─────────────────────────────────────────────
    addVpn(makeComboRow(vpnCard,
                        QStringLiteral("NetShield Ad-blocker"),
                        QStringLiteral("Block malware, ads, and trackers at the DNS level."),
                        QStringLiteral("netshield"),
                        {
                            QStringLiteral("Off"),
                            QStringLiteral("Malware only"),
                            QStringLiteral("Malware, ads & trackers")
                        },
                        {
                            QStringLiteral("off"),
                            QStringLiteral("malware-only"),
                            QStringLiteral("malware-ads-trackers")
                        }));

    // ── Port Forwarding ───────────────────────────────────────
    addVpn(makeToggleRow(vpnCard,
                         QStringLiteral("Port Forwarding"),
                         QStringLiteral("Bypass firewalls to connect to P2P servers and devices in your local network. "
                             "<a href='https://protonvpn.com/support/port-forwarding'>Learn more</a> · "
                             "<a href='https://protonvpn.com/support/port-forwarding-manual-setup#linux'>Guide</a>"),
                         QStringLiteral("port-forwarding")));

    // ── Custom DNS ────────────────────────────────────────────
    {
        addDivider(vpnCardLayout, vpnCard);

        auto* dnsRow = new QWidget(vpnCard);
        auto* dnsRl = new QHBoxLayout(dnsRow);
        dnsRl->setContentsMargins(16, 12, 16, 4);
        dnsRl->setSpacing(16);
        dnsRl->addWidget(makeTextCol(dnsRow,
                                     QStringLiteral("Custom DNS"),
                                     QStringLiteral("Override the VPN DNS with your own resolver(s). "
                                         "Separate multiple addresses with a comma.")), 1);
        m_dnsToggle = new ToggleSwitch(dnsRow);
        dnsRl->addWidget(m_dnsToggle);
        vpnCardLayout->addWidget(dnsRow);

        auto* dnsAddrRow = new QWidget(vpnCard);
        dnsAddrRow->setVisible(false);
        auto* dnsAddrRl = new QHBoxLayout(dnsAddrRow);
        dnsAddrRl->setContentsMargins(16, 0, 16, 12);
        m_dnsEdit = new QLineEdit(dnsAddrRow);
        m_dnsEdit->setPlaceholderText(QStringLiteral("e.g. 1.1.1.1,8.8.8.8"));
        m_dnsEdit->setObjectName(QStringLiteral("settingsDnsEdit"));
        dnsAddrRl->addWidget(m_dnsEdit);
        m_dnsApplyBtn = new QPushButton(QStringLiteral("Apply"), dnsAddrRow);
        m_dnsApplyBtn->setObjectName(QStringLiteral("secondaryButton"));
        m_dnsApplyBtn->setFixedHeight(28);
        dnsAddrRl->addWidget(m_dnsApplyBtn);
        vpnCardLayout->addWidget(dnsAddrRow);

        connect(m_dnsToggle, &ToggleSwitch::toggled, this, [this, dnsAddrRow](bool on)
        {
            dnsAddrRow->setVisible(on);
            if (!on)
                m_manager->applyConfigValue(QStringLiteral("custom-dns"), QStringLiteral("off"));
            // Reconnect detection handled via the configApplied signal.
        });
        connect(m_dnsApplyBtn, &QPushButton::clicked, this, [this]()
        {
            const QString dns = m_dnsEdit->text().trimmed();
            if (dns.isEmpty()) return;
            m_manager->applyConfigValue(
                QStringLiteral("custom-dns"),
                QStringLiteral("--dns %1 on").arg(dns));
            // Reconnect detection handled via the configApplied signal.
        });
    }

    vpnCardLayout->addStretch();


    // VpnManager signals
    connect(m_manager, &VpnManager::settingsReady, this, &SettingsPage::onSettingsReady);
    connect(m_manager, &VpnManager::configApplied, this, &SettingsPage::maybeWarnReconnect);

    // Spinner timer
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % kSpinnerFrameCount;
        m_statusLabel->setText(
            QStringLiteral("%1 Loading settings…").arg(QString::fromUtf8(kSpinnerFrames[m_spinnerFrame])));
    });
}

// ============================================================
// SettingsPage slots
// ============================================================

void SettingsPage::refresh()
{
    setLoading(true);
    m_manager->fetchSettings();
}

void SettingsPage::setLoading(const bool loading)
{
    m_loading = loading;
    m_refreshBtn->setEnabled(!loading);
    m_refreshBtn->setText(loading ? QStringLiteral("Loading…") : QStringLiteral("↻ Refresh"));
    m_statusLabel->setVisible(loading);
    if (loading)
    {
        m_spinnerFrame = 0;
        m_statusLabel->setText(QStringLiteral("⠋ Loading settings…"));
        m_spinnerTimer->start();
    }
    else
    {
        m_spinnerTimer->stop();
    }
    for (const auto& r : std::as_const(m_toggleRows)) r.toggle->setEnabled(!loading);
    for (const auto& r : std::as_const(m_comboRows)) r.combo->setEnabled(!loading);
    if (m_autoStartToggle) m_autoStartToggle->setEnabled(!loading);
    if (m_notificationsToggle) m_notificationsToggle->setEnabled(!loading);
    if (m_recentConnectionsSpinBox) m_recentConnectionsSpinBox->setEnabled(!loading);
    if (m_dnsToggle) m_dnsToggle->setEnabled(!loading);
    if (m_dnsApplyBtn) m_dnsApplyBtn->setEnabled(!loading);
}

void SettingsPage::onSettingsReady(const QMap<QString, QString>& info)
{
    setLoading(false);

    auto val = [&](const QString& key)
    {
        return info.value(key).toLower().trimmed();
    };

    // Toggle rows
    for (const auto& row : std::as_const(m_toggleRows))
        row.toggle->setOn(isOnString(val(row.cliKey)), false);

    // Combo rows – find the matching CLI value and select that index
    for (const auto& row : std::as_const(m_comboRows))
    {
        const QString v = val(row.cliKey);
        int idx = row.cliValues.indexOf(v);
        if (idx < 0) idx = 0;
        // Block signals so we don't fire applyConfigValue on load
        row.combo->blockSignals(true);
        row.combo->setCurrentIndex(idx);
        row.combo->blockSignals(false);
    }

    // Custom DNS
    const QString dns = info.value(QStringLiteral("custom-dns")).trimmed();
    const bool dnsOn = !dns.isEmpty()
        && dns.toLower() != QLatin1String("disabled")
        && dns.toLower() != QLatin1String("off")
        && dns.toLower() != QLatin1String("none");
    m_dnsToggle->setOn(dnsOn, false);
    if (dnsOn) m_dnsEdit->setText(dns);
    else m_dnsEdit->clear();
}

void SettingsPage::showAboutDialog()
{
    // Load versions from the embedded version.json resource
    QString appVersion = QStringLiteral("unknown");
    QString cliVersion = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
            appVersion = obj[QStringLiteral("app_version")].toString();
        if (obj.contains(QStringLiteral("cli_version_tested")))
            cliVersion = obj[QStringLiteral("cli_version_tested")].toString();
    }

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("About ProtonVPN Qt App"));
    dlg->setMinimumSize(520, 400);

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(16);

    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(QStringLiteral(R"(
<h2 style="margin-bottom:4px;">ProtonVPN Qt App</h2>
<p style="color:#888;margin-top:0;">A community-built Qt front-end for the Proton VPN CLI.</p>
<table style="margin-bottom:8px;">
  <tr><td><b>App version:&nbsp;</b></td><td>%1</td></tr>
  <tr><td><b>Tested against CLI:&nbsp;</b></td><td>%2</td></tr>
</table>
<p><b>⚠ Disclaimer:</b> This project is <b>not affiliated with, endorsed by, or
supported by Proton AG</b> in any way. ProtonVPN and the Proton logo are
trademarks of Proton AG.</p>
<hr/>
<h3>Author</h3>
<ul>
  <li>Nicholas Page (<a href="https://github.com/wheat32">wheat32</a>)</li>
</ul>
<h3>Credits &amp; Acknowledgements</h3>
<ul>
  <li>Built with <a href="https://www.qt.io/">Qt 6</a></li>
  <li>Uses the <a href="https://protonvpn.com/support/linux-vpn-tool/">ProtonVPN Linux CLI</a></li>
  <li>Icons from <a href="https://icons.getbootstrap.com/">Bootstrap Icons</a>
      (MIT License)</li>
  <li>Country flag SVGs from <a href="https://github.com/lipis/flag-icons">flag-icons</a>
      by Panayiotis Lipiridis (MIT License)</li>
</ul>
<hr/>
<p style="color:#888;font-size:small;">
  This software is provided as-is, without warranty of any kind. Use at your own risk.
</p>
)").arg(appVersion, cliVersion));
    layout->addWidget(browser);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
    layout->addWidget(btns);

    dlg->exec();
    dlg->deleteLater();
}




