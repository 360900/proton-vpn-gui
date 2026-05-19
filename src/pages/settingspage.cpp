#include "settingspage.h"
#include "../appconfig.h"
#include "../thememanager.h"
#include "../connectionhistory.h"
#include "../uihelpers.h"
#include "../widgets/togglewithstatus.h"
#include "../cli/flatpakutils.h"

#include <QSpinBox>
#include <QVBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include "../widgets/numberspinner.h"
#include "../widgets/toastnotification.h"
#include <QGraphicsOpacityEffect>
#include <QDialogButtonBox> // ignore unused include warning for QDialogButtonBox
#include <QTextBrowser>
#include <QMessageBox>
#include <QStyle>
#include <QFontDatabase>
#include <QClipboard>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QVersionNumber>
#include <QGridLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDebug>
#include <algorithm>
#include <optional>


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

QWidget* SettingsPage::makePlusDivider(QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* hl = new QHBoxLayout(container);
    hl->setContentsMargins(16, 10, 16, 10);
    hl->setSpacing(10);

    auto makeHLine = [&]() -> QFrame*
    {
        auto* line = new QFrame(container);
        line->setFrameShape(QFrame::HLine);
        line->setObjectName(QStringLiteral("plusDividerLine"));
        return line;
    };

    hl->addWidget(makeHLine(), 1);

    auto* label = new QLabel(tr("✦  Available to Plus Members"), container);
    label->setObjectName(QStringLiteral("plusDividerLabel"));
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    hl->addWidget(label);

    hl->addWidget(makeHLine(), 1);

    return container;
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
    if (desc.isEmpty() == false)
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

    if (needsReconnect == false) return;
    if (m_manager->currentState() != VpnState::Connected) return;

    QMessageBox mb(this);
    mb.setWindowTitle(tr("Reconnect Required"));
    mb.setIcon(QMessageBox::Information);
    mb.setText(tr("This setting change will only take effect after reconnecting to the VPN."));
    mb.setStandardButtons(QMessageBox::Ok);
    mb.exec();
}

void SettingsPage::showReconnectDialog(const QString& settingLabel,
                                       std::function<void()> onAccept)
{
    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(tr("Reconnect Required"));
    dlg->setModal(true);
    dlg->setMinimumWidth(440);

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 20);

    auto* heading = new QLabel(
        QStringLiteral("<b>%1</b>")
            .arg(tr("%1 \u2014 Reconnect Required").arg(settingLabel).toHtmlEscaped()),
        dlg);
    heading->setTextFormat(Qt::RichText);

    auto* body = new QLabel(
        tr("Changing this setting requires reconnecting to the VPN.\n\n"
           "You can disconnect, apply the change, and reconnect to the same "
           "location automatically, or dismiss this dialog and do it manually."),
        dlg);
    body->setWordWrap(true);

    layout->addWidget(heading);
    layout->addWidget(body);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    auto* dismissBtn = new QPushButton(tr("Dismiss"), dlg);
    dismissBtn->setObjectName(QStringLiteral("secondaryButton"));
    connect(dismissBtn, &QPushButton::clicked, dlg, &QDialog::reject);

    auto* reconnectBtn = new QPushButton(
        tr("Apply && Reconnect"), dlg);
    reconnectBtn->setObjectName(QStringLiteral("primaryButton"));
    reconnectBtn->setDefault(true);

    const int btnH = reconnectBtn->sizeHint().height();
    dismissBtn->setFixedHeight(btnH);
    reconnectBtn->setFixedHeight(btnH);

    connect(reconnectBtn, &QPushButton::clicked, dlg,
            [dlg, onAccept = std::move(onAccept)]()
    {
        dlg->accept();
        onAccept();
    });

    btnRow->addWidget(dismissBtn, 1);
    btnRow->addWidget(reconnectBtn, 1);
    layout->addLayout(btnRow);

    dlg->exec();
}

QWidget* SettingsPage::makeToggleRow(QWidget* parent, const QString& label,
                                     const QString& desc, const QString& cliKey,
                                     const QString& onValue, const bool requiresReconnect)
{
    auto* row = new QWidget(parent);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(16, 12, 16, 12);
    rl->setSpacing(16);
    rl->addWidget(makeTextCol(row, label, desc), 1);
    auto* toggle = new ToggleWithStatus(row);
    rl->addWidget(toggle, 0);

    connect(toggle, &ToggleWithStatus::toggled, this,
            [this, cliKey, onValue, label, toggle, requiresReconnect](bool on)
    {
        if (requiresReconnect && m_manager->currentState() != VpnState::Disconnected)
        {
            // Revert the toggle immediately — it will be flipped back optimistically
            // if the user accepts the dialog.
            toggle->blockSignals(true);
            toggle->setOn(!on, false);
            toggle->blockSignals(false);

            const QString newValue = on ? onValue : QStringLiteral("off");
            showReconnectDialog(label, [this, toggle, on, cliKey, newValue]()
            {
                // Optimistically show the intended new state while reconnecting.
                toggle->blockSignals(true);
                toggle->setOn(on, true);
                toggle->blockSignals(false);
                m_sequencePending = true;
                m_manager->applyConfigValueAndReconnect(cliKey, newValue);
            });
            return;
        }
        m_manager->applyConfigValue(cliKey, on ? onValue : QStringLiteral("off"));
    });

    m_toggleRows.append({cliKey, toggle, onValue});
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

    connect(combo, &QComboBox::currentIndexChanged,
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
    // Check once whether systemctl is reachable (on the host when in Flatpak).
    static std::optional<bool> cached;
    if (cached.has_value()) return *cached;

    QProcess p;
    auto [prog, args] = buildHostCommand(QStringLiteral("systemctl"),
                                         {QStringLiteral("--version")});
    p.start(prog, args);
    cached = p.waitForStarted(2000) && p.waitForFinished(2000);
    return *cached;
}

bool SettingsPage::autoStartEnabled()
{
    if (kDryRun) return false; // dry-run: always report disabled
    if (systemdAvailable() == false) return false;

    // Ask systemd whether the service is enabled. Exit code 0 = enabled.
    QProcess p;
    auto [prog, args] = buildHostCommand(QStringLiteral("systemctl"),
                                         {QStringLiteral("--user"),
                                          QStringLiteral("is-enabled"),
                                          kServiceName});
    p.start(prog, args);
    if (p.waitForFinished(3000) == false) return false;
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
        // When running as a Flatpak the launcher must be `flatpak run <app-id>`
        // rather than the sandbox-internal binary path, so the service works
        // even when the app is not running from within the sandbox at login.
        const QString exe = isRunningAsFlatpak()
            ? QStringLiteral("flatpak run io.github.wheat32.ProtonVPNQt")
            : QCoreApplication::applicationFilePath();

        // Create the systemd user service directory if it doesn't exist.
        if (QDir().mkpath(kSystemdUserServiceDir) == false)
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
        auto [prog, args] = buildHostCommand(QStringLiteral("systemctl"),
                                             {QStringLiteral("--user"),
                                              QStringLiteral("enable"),
                                              kServiceName});
        p.start(prog, args);
        p.waitForFinished(5000);
        if (p.exitCode() != 0)
        {
            errorOut = QString::fromUtf8(p.readAllStandardError()).trimmed();
            if (errorOut.isEmpty())
            {
                errorOut = QStringLiteral("systemctl --user enable failed (exit %1)").arg(p.exitCode());
            }

            return false;
        }
    }
    else
    {
        // Disable and remove.
        QProcess p;
        auto [prog, args] = buildHostCommand(QStringLiteral("systemctl"),
                                             {QStringLiteral("--user"),
                                              QStringLiteral("disable"),
                                              kServiceName});
        p.start(prog, args);
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
    if (m_autoConnectRow == nullptr) return;
    const bool show = m_autoStartToggle != nullptr && m_autoStartToggle->isOn();
    m_autoConnectRow->setVisible(show);
    // If auto-start is turned off, also disable auto-connect and persist that.
    if (show == false && m_autoConnectToggle != nullptr && m_autoConnectToggle->isOn())
    {
        m_autoConnectToggle->setOn(false, false);
        AppConfig::instance().setAutoConnect(false);
    }
}

SettingsPage::SettingsPage(VpnManager* manager, NatPmpManager* natPmpManager, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_natPmpManager(natPmpManager)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 16, 16, 16);
    outerLayout->setSpacing(12);

    // Title
    auto* titleLabel = new QLabel(tr("Settings"), this);
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
    tabs->addTab(appTab, tr("App"));

    bool appFirst = true;
    auto addApp = [&](QWidget* w)
    {
        if (appFirst == false)
        {
            addDivider(appCardLayout, appCard);
        }
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
                                  tr("Launch on Startup"),
                                  tr("Automatically start the app in the background when you log in "
                                     "(installs a systemd user service).")), 1);
        m_autoStartToggle = new ToggleWithStatus(m_autoStartRow);
        m_autoStartToggle->setOn(autoStartEnabled(), false);
        connect(m_autoStartToggle, &ToggleWithStatus::toggled, this, [this](bool on)
        {
            QString err;
            if (!setAutoStart(on, err))
            {
                m_autoStartToggle->setOn(!on, false);
                QMessageBox::warning(this,
                                     tr("Auto-start Error"),
                                     tr("Failed to %1 auto-start:\n%2")
                                     .arg(on ? tr("enable") : tr("disable"), err));
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
                                    tr("Auto-connect on Startup"),
                                    tr("Automatically connect to the VPN when the app starts.")), 1);
        m_autoConnectToggle = new ToggleWithStatus(m_autoConnectRow);
        m_autoConnectToggle->setOn(AppConfig::instance().autoConnect(), false);
        connect(m_autoConnectToggle, &ToggleWithStatus::toggled, this, [](bool on)
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
                                  tr("Desktop Notifications"),
                                  tr("Show a system notification when the VPN is connecting, "
                                     "connected, disconnecting, or disconnected.")), 1);
        m_notificationsToggle = new ToggleWithStatus(row);
        m_notificationsToggle->setOn(AppConfig::instance().notifications(), false);
        connect(m_notificationsToggle, &ToggleWithStatus::toggled, this, [](bool on)
        {
            AppConfig::instance().setNotifications(on);
        });
        rl->addWidget(m_notificationsToggle);
        addApp(row);
    }

    // ── Theme ─────────────────────────────────────────────────
    {
        auto* row = new QWidget(appCard);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(row,
                                  tr("Theme"),
                                  tr("Choose the colour scheme for the app.")), 1);
        m_themeCombo = new QComboBox(row);
        m_themeCombo->addItem(tr("System Settings"), QStringLiteral("system"));
        m_themeCombo->addItem(tr("Dark"),            QStringLiteral("dark"));
        m_themeCombo->addItem(tr("Light"),           QStringLiteral("light"));

        // Select current saved value
        {
            const AppConfig::Theme t = AppConfig::instance().theme();
            const int idx = (t == AppConfig::Theme::Dark)  ? 1 :
                            (t == AppConfig::Theme::Light) ? 2 : 0;
            m_themeCombo->setCurrentIndex(idx);
        }

        connect(m_themeCombo, &QComboBox::currentIndexChanged, this, [this](int idx)
        {
            const AppConfig::Theme t = (idx == 1) ? AppConfig::Theme::Dark  :
                                       (idx == 2) ? AppConfig::Theme::Light :
                                                    AppConfig::Theme::System;
            AppConfig::instance().setTheme(t);
            ThemeManager::apply(t);
        });

        rl->addWidget(m_themeCombo);
        addApp(row);
    }

    // ── Start Hidden ───────────────────────────────────────────
    {
        auto* row = new QWidget(appCard);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(row,
                                  tr("Start Hidden"),
                                  tr("Launch the app in the background without opening a "
                                     "window. Access it anytime via the system tray icon.")), 1);
        auto* toggle = new ToggleWithStatus(row);
        toggle->setOn(AppConfig::instance().startHidden(), false);
        connect(toggle, &ToggleWithStatus::toggled, this, [](bool on)
        {
            AppConfig::instance().setStartHidden(on);
        });
        rl->addWidget(toggle);
        addApp(row);
    }

    // ── Plus Members Only divider (App tab) ───────────────────
    m_appPlusDivider = makePlusDivider(appCard);
    appCardLayout->addWidget(m_appPlusDivider);

    // ── PLUS-ONLY section (App tab) ───────────────────────────
    m_appPlusSection = new QWidget(appCard);
    auto* appPlusLayout = new QVBoxLayout(m_appPlusSection);
    appPlusLayout->setContentsMargins(0, 0, 0, 0);
    appPlusLayout->setSpacing(0);
    appCardLayout->addWidget(m_appPlusSection);

    bool appPlusFirst = true;
    auto addAppPlus = [&](QWidget* w)
    {
        if (!appPlusFirst) addDivider(appPlusLayout, m_appPlusSection);
        appPlusFirst = false;
        appPlusLayout->addWidget(w);
    };

    // ── Recent Connections ────────────────────────────────────
    {
        auto* row = new QWidget(m_appPlusSection);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->setSpacing(16);
        rl->addWidget(makeTextCol(row,
                                  tr("Recent Connections"),
                                  tr("Number of recent VPN connections to remember and show "
                                     "on the home screen. Set to 0 to disable.")), 1);
        m_recentConnectionsSpinBox = new NumberSpinner(row);
        m_recentConnectionsSpinBox->setRange(0, 20);
        m_recentConnectionsSpinBox->setValue(AppConfig::instance().recentConnectionsCount());
        connect(m_recentConnectionsSpinBox, &NumberSpinner::valueChanged, this, [](const int val)
        {
            AppConfig::instance().setRecentConnectionsCount(val);
        });
        rl->addWidget(m_recentConnectionsSpinBox);
        addAppPlus(row);
    }

    // ── Clear Recent Connections (only shown when history is non-empty) ────
    {
        m_clearRecentRow = new QWidget(m_appPlusSection);
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
                                  tr("Clear Recent Connections"),
                                  tr("Remove all saved recent connection history.")), 1);
        auto* clearBtn = new QPushButton(tr("Clear"), inner);
        clearBtn->setObjectName(QStringLiteral("dangerButton"));
        clearBtn->setCursor(Qt::PointingHandCursor);
        connect(clearBtn, &QPushButton::clicked, this, [this]()
        {
            ConnectionHistory::instance().clear();
            m_clearRecentRow->setVisible(false);
            emit recentConnectionsCleared();
            ToastNotification::popup(this, tr("Recent connection history cleared."));
        });
        rl->addWidget(clearBtn);
        cLayout->addWidget(inner);

        appPlusLayout->addWidget(m_clearRecentRow);
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
        auto* aboutBtn = new QPushButton(tr("About"), appTab);
        aboutBtn->setObjectName(QStringLiteral("secondaryButton"));
        aboutBtn->setCursor(Qt::PointingHandCursor);
        connect(aboutBtn, &QPushButton::clicked, this, [this]() {
            AboutDialog dlg(m_installedCliVersion, this);
            dlg.exec();
        });
        appPageLayout->addWidget(aboutBtn);
    }

    // ============================================================
    // TAB 2 – VPN
    // ============================================================
    auto* vpnTab = new QWidget();
    auto [vpnCard, vpnCardLayout] = makeCard(vpnTab);
    m_vpnCard = vpnCard; // kept so we can bulk-disable during VPN transitions

    // Refresh button + spinner live inside the VPN tab's page layout
    {
        auto* vpnPageLayout = qobject_cast<QVBoxLayout*>(vpnTab->layout());

        auto* headerRow = new QHBoxLayout();
        m_refreshBtn = new QPushButton(tr("↻ Refresh"), vpnTab);
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

    tabs->addTab(vpnTab, tr("VPN"));

    // ── FREE rows (available to all plans) ───────────────────
    bool vpnFirst = true;
    auto addVpn = [&](QWidget* w)
    {
        if (!vpnFirst) addDivider(vpnCardLayout, vpnCard);
        vpnFirst = false;
        vpnCardLayout->addWidget(w);
    };

    // ── Anonymous Crash Reports ───────────────────────────────
    addVpn(makeToggleRow(vpnCard,
                         tr("Anonymous Crash Reports"),
                         tr("Send anonymous crash reports to Proton for the VPN CLI tool — not this Qt app."),
                         QStringLiteral("anonymous-crash-reports")));

    // ── IPv6 ─────────────────────────────────────────────────
    addVpn(makeToggleRow(vpnCard,
                         tr("IPv6"),
                         tr("Enable IPv6 support over the VPN tunnel."),
                         QStringLiteral("ipv6"),
                         QStringLiteral("on"),
                         /*requiresReconnect=*/true));

    // ── Kill Switch ───────────────────────────────────────────
    {
        // Outer container: toggle row on top + collapsible radio sub-panel below.
        auto* ksContainer = new QWidget(vpnCard);
        auto* ksVLayout = new QVBoxLayout(ksContainer);
        ksVLayout->setContentsMargins(0, 0, 0, 0);
        ksVLayout->setSpacing(0);

        // --- Toggle row ---
        auto* ksRow = new QWidget(ksContainer);
        auto* ksRowLayout = new QHBoxLayout(ksRow);
        ksRowLayout->setContentsMargins(16, 12, 16, 12);
        ksRowLayout->setSpacing(16);
        ksRowLayout->addWidget(makeTextCol(ksRow,
            tr("Kill Switch"),
            tr("Block internet access if the VPN connection drops unexpectedly.")), 1);
        m_killSwitchToggle = new ToggleWithStatus(ksRow);
        ksRowLayout->addWidget(m_killSwitchToggle, 0);
        ksVLayout->addWidget(ksRow);

        // --- Sub-panel: Standard / Advanced radio buttons ---
        m_killSwitchSubPanel = new QWidget(ksContainer);
        m_killSwitchSubPanel->setVisible(false);
        auto* spLayout = new QVBoxLayout(m_killSwitchSubPanel);
        spLayout->setContentsMargins(16, 0, 16, 12);
        spLayout->setSpacing(0);

        // Thin separator between toggle row and radio options
        auto* spSep = new QFrame(m_killSwitchSubPanel);
        spSep->setFrameShape(QFrame::HLine);
        spSep->setObjectName(QStringLiteral("divider"));
        spLayout->addWidget(spSep);

        auto* radioGroup = new QButtonGroup(m_killSwitchSubPanel);

        // Helper: build a radio-button option row and append it to spLayout
        auto makeKsOption = [&](const QString& title, const QString& desc,
                                bool enabled, bool checked) -> QRadioButton*
        {
            auto* optRow = new QWidget(m_killSwitchSubPanel);
            auto* hl = new QHBoxLayout(optRow);
            hl->setContentsMargins(0, 10, 0, 10);
            hl->setSpacing(10);

            auto* radio = new QRadioButton(optRow);
            radio->setChecked(checked);
            radio->setEnabled(enabled);
            hl->addWidget(radio, 0, Qt::AlignTop);

            auto* textCol = new QWidget(optRow);
            auto* vl = new QVBoxLayout(textCol);
            vl->setContentsMargins(0, 0, 0, 0);
            vl->setSpacing(2);

            auto* titleLbl = new QLabel(title, textCol);
            titleLbl->setObjectName(QStringLiteral("infoKey"));
            if (!enabled)
                titleLbl->setStyleSheet(QStringLiteral("color: #555;"));
            vl->addWidget(titleLbl);

            auto* descLbl = new QLabel(desc, textCol);
            descLbl->setObjectName(QStringLiteral("settingsDesc"));
            descLbl->setWordWrap(true);
            QFont f = descLbl->font();
            f.setPointSize(qMax(f.pointSize() - 1, 7));
            descLbl->setFont(f);
            descLbl->setStyleSheet(enabled
                ? QStringLiteral("color: #888;")
                : QStringLiteral("color: #444;"));
            vl->addWidget(descLbl);

            hl->addWidget(textCol, 1);
            spLayout->addWidget(optRow);
            return radio;
        };

        // Standard (enabled, always selected)
        auto* standardRadio = makeKsOption(
            tr("Standard"),
            tr("Automatically disconnect from the internet if the VPN connection is lost."),
            true, true);
        radioGroup->addButton(standardRadio);

        // Divider between options
        auto* optSep = new QFrame(m_killSwitchSubPanel);
        optSep->setFrameShape(QFrame::HLine);
        optSep->setObjectName(QStringLiteral("divider"));
        spLayout->addWidget(optSep);

        // Advanced (disabled – temporarily removed from the CLI)
        auto* advancedRadio = makeKsOption(
            tr("Advanced"),
            tr("Only allow internet access when connected to ProtonVPN. "
               "Advanced kill switch will remain active even when you restart your device."),
            false, false);
        radioGroup->addButton(advancedRadio);
        const QString advTooltip = tr(
            "Temporarily removed from the Proton VPN CLI — not currently available.");
        advancedRadio->setToolTip(advTooltip);
        // Propagate the tooltip to the whole row so hovering anywhere on it shows it
        advancedRadio->parentWidget()->setToolTip(advTooltip);

        ksVLayout->addWidget(m_killSwitchSubPanel);

        // Wire toggle → apply CLI value + show/hide sub-panel
        connect(m_killSwitchToggle, &ToggleWithStatus::toggled, this, [this](bool on)
        {
            // The CLI refuses to change kill-switch while the VPN is active.
            // Guard against any non-Disconnected state.
            if (m_manager->currentState() != VpnState::Disconnected)
            {
                // Revert the toggle back without re-emitting the signal.
                m_killSwitchToggle->blockSignals(true);
                m_killSwitchToggle->setOn(!on, false);
                m_killSwitchToggle->blockSignals(false);
                m_killSwitchSubPanel->setVisible(!on);

                showReconnectDialog(tr("Kill Switch"), [this, on]()
                {
                    // Optimistically set the toggle to the new state.
                    m_killSwitchToggle->blockSignals(true);
                    m_killSwitchToggle->setOn(on, true);
                    m_killSwitchToggle->blockSignals(false);
                    m_killSwitchSubPanel->setVisible(on);
                    // Lock the whole VPN card until the sequence completes.
                    m_sequencePending = true;
                    const QString val = on ? QStringLiteral("standard")
                                           : QStringLiteral("off");
                    m_manager->applyConfigValueAndReconnect(
                        QStringLiteral("kill-switch"), val);
                });
                return;
            }

            m_killSwitchSubPanel->setVisible(on);
            m_manager->applyConfigValue(QStringLiteral("kill-switch"),
                                        on ? QStringLiteral("standard") : QStringLiteral("off"));
        });

        addVpn(ksContainer);
    }

    // ── Plus Members Only divider ─────────────────────────────
    m_plusDivider = makePlusDivider(vpnCard);
    vpnCardLayout->addWidget(m_plusDivider);

    // ── PLUS-ONLY section ─────────────────────────────────────
    m_plusSection = new QWidget(vpnCard);
    auto* plusLayout = new QVBoxLayout(m_plusSection);
    plusLayout->setContentsMargins(0, 0, 0, 0);
    plusLayout->setSpacing(0);
    vpnCardLayout->addWidget(m_plusSection);

    bool plusFirst = true;
    auto addPlus = [&](QWidget* w)
    {
        if (plusFirst == false)
        {
            addDivider(plusLayout, m_plusSection);
        }

        plusFirst = false;
        plusLayout->addWidget(w);
    };

    // ── NAT Type ──────────────────────────────────────────────
    addPlus(makeComboRow(m_plusSection,
                         tr("NAT Type"),
                         tr("Controls how the VPN server maps your connection. "
                            "<b>Strict (Type 3)</b> is the default and best for privacy. "
                            "<b>Moderate (Type 2)</b> improves compatibility for online gaming and WebRTC, "
                            "at a slight privacy trade-off. "
                            "<a href='https://protonvpn.com/support/moderate-nat'>Read more</a>"),
                         QStringLiteral("moderate-nat"),
                         {tr("Strict (Type 3)"),
                          tr("Moderate (Type 2)")},
                         {QStringLiteral("off"), QStringLiteral("on")}));

    // ── VPN Accelerator ───────────────────────────────────────
    addPlus(makeToggleRow(m_plusSection,
                          tr("VPN Accelerator"),
                          tr("Boost connection speeds using advanced protocol techniques."),
                          QStringLiteral("vpn-accelerator")));

    // ── NetShield ─────────────────────────────────────────────
    addPlus(makeComboRow(m_plusSection,
                         tr("NetShield Ad-blocker"),
                         tr("Block malware, ads, and trackers at the DNS level."),
                         QStringLiteral("netshield"),
                         {
                             tr("Off"),
                             tr("Malware only"),
                             tr("Malware, ads & trackers")
                         },
                         {
                             QStringLiteral("off"),
                             QStringLiteral("malware-only"),
                             QStringLiteral("malware-ads-trackers")
                         }));

    // ── Port Forwarding ───────────────────────────────────────
    {
        QWidget* pfRow = makeToggleRow(m_plusSection,
                                    tr("Port Forwarding"),
                                    tr("Bypass firewalls to connect to P2P servers and devices in your local network. "
                                       "<a href='https://protonvpn.com/support/port-forwarding'>Learn more</a>"),
                                    QStringLiteral("port-forwarding"));
        // Grab the toggle widget that makeToggleRow just appended to m_toggleRows.
        m_portForwardingToggle = m_toggleRows.last().toggle;

        // Show a warning popup if the user enables port forwarding without natpmpc installed.
        connect(m_portForwardingToggle, &ToggleWithStatus::toggled, this, [this](bool on)
        {
            if (on && NatPmpManager::isInstalled() == false)
            {
                auto* dlg = new QDialog(this);
                dlg->setWindowTitle(tr("natpmpc Not Installed"));
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->setMinimumWidth(480);

                auto* layout = new QVBoxLayout(dlg);
                layout->setSpacing(12);
                layout->setContentsMargins(20, 20, 20, 16);

                // Icon + title row
                auto* titleRow = new QHBoxLayout();
                auto* iconLabel = new QLabel(dlg);
                iconLabel->setPixmap(dlg->style()->standardIcon(QStyle::SP_MessageBoxWarning)
                                         .pixmap(32, 32));
                titleRow->addWidget(iconLabel);
                titleRow->addSpacing(8);
                QLabel* titleLabel = new QLabel(
                    QStringLiteral("<b>%1</b>")
                        .arg(tr("natpmpc is not installed.").toHtmlEscaped()),
                    dlg);
                titleLabel->setTextFormat(Qt::RichText);
                titleRow->addWidget(titleLabel, 1);
                layout->addLayout(titleRow);

                // Description
                auto* descLabel = new QLabel(
                    tr("Port forwarding requires the <code>natpmpc</code> binary to display "
                       "and keep the forwarded port alive. Without it, the forwarded port "
                       "will not be shown in the app.<br><br>"
                       "Install it using the command for your distribution:"),
                    dlg);
                descLabel->setTextFormat(Qt::RichText);
                descLabel->setWordWrap(true);
                layout->addWidget(descLabel);

                // Build the clipboard icon once from the SVG asset, tinted white.
                const auto makeClipboardIcon = [](const int size) -> QIcon
                {
                    QPixmap pix(size, size);
                    pix.fill(Qt::transparent);
                    QPainter p(&pix);
                    QSvgRenderer renderer(QStringLiteral(":/assets/clipboard2-plus.svg"));
                    renderer.render(&p);
                    // Tint every opaque pixel white.
                    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
                    p.fillRect(pix.rect(), Qt::white);
                    p.end();
                    return QIcon(pix);
                };
                const QIcon clipIcon = makeClipboardIcon(16);

                // Helper lambda to add a labeled read-only command input with a copy button.
                auto addCmd = [&](const QString& distro, const QString& cmd)
                {
                    layout->addWidget(new QLabel(distro, dlg));

                    auto* row  = new QWidget(dlg);
                    auto* hbox = new QHBoxLayout(row);
                    hbox->setContentsMargins(0, 0, 0, 0);
                    hbox->setSpacing(4);

                    auto* edit = new QLineEdit(cmd, row);
                    edit->setReadOnly(true);
                    edit->setObjectName(QStringLiteral("codeInput"));
                    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
                    hbox->addWidget(edit, 1);

                    auto* copyBtn = new QPushButton(row);
                    copyBtn->setIcon(clipIcon);
                    copyBtn->setIconSize({16, 16});
                    copyBtn->setFixedSize(28, 28);
                    copyBtn->setToolTip(tr("Copy to Clipboard"));
                    copyBtn->setStyleSheet(QStringLiteral(
                        "QPushButton {"
                        "  border: 1px solid palette(mid);"
                        "  border-radius: 4px;"
                        "  padding: 2px;"
                        "  background: transparent;"
                        "}"
                        "QPushButton:hover {"
                        "  border-color: palette(highlight);"
                        "  background: rgba(255,255,255,15);"
                        "}"
                        "QPushButton:pressed {"
                        "  background: rgba(255,255,255,30);"
                        "}"));
                    connect(copyBtn, &QPushButton::clicked, dlg, [cmd]()
                    {
                        QApplication::clipboard()->setText(cmd);
                    });
                    hbox->addWidget(copyBtn);

                    layout->addWidget(row);
                };

                addCmd(tr("Debian / Ubuntu:"), QStringLiteral("sudo apt install natpmpc"));
                addCmd(tr("Fedora:"),          QStringLiteral("sudo dnf install libnatpmp"));
                addCmd(tr("Arch Linux:"),      QStringLiteral("sudo pacman -S libnatpmp"));

                // OK button
                auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
                connect(btnBox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
                layout->addSpacing(4);
                layout->addWidget(btnBox);

                dlg->exec();
            }
        });

        // Hide the forwarded port row when port forwarding is turned off.
        connect(m_portForwardingToggle, &ToggleWithStatus::toggled, this, [this](const bool on)
        {
            if (on == false && m_settingsPortRow != nullptr)
            {
                m_settingsPortRow->setVisible(false);
            }
        });

        addPlus(pfRow);

        // ── Forwarded port display ────────────────────────────────────────
        // Shown below the toggle when port forwarding is on and natpmpc is installed.
        m_settingsPortRow = new QWidget(m_plusSection);
        m_settingsPortRow->setObjectName(QStringLiteral("settingsPortRow"));
        auto* portRowLayout = new QHBoxLayout(m_settingsPortRow);
        portRowLayout->setContentsMargins(16, 0, 16, 12);
        portRowLayout->setSpacing(8);

        auto* portTitleLabel = new QLabel(tr("Forwarded Port:"), m_settingsPortRow);
        portTitleLabel->setObjectName(QStringLiteral("infoLabel"));
        portRowLayout->addWidget(portTitleLabel, 0, Qt::AlignVCenter);

        // Button group
        auto* btnGroup = new QWidget(m_settingsPortRow);
        auto* btnGroupLayout = new QHBoxLayout(btnGroup);
        btnGroupLayout->setContentsMargins(0, 0, 0, 0);
        btnGroupLayout->setSpacing(0);

        m_settingsPortLabel = new QLabel(QStringLiteral("—"), btnGroup);
        m_settingsPortLabel->setObjectName(QStringLiteral("portValueLabel"));
        m_settingsPortLabel->setAlignment(Qt::AlignCenter);
        {
            QFont f = m_settingsPortLabel->font();
            f.setBold(true);
            f.setPointSize(f.pointSize() + 1);
            m_settingsPortLabel->setFont(f);
        }
        btnGroupLayout->addWidget(m_settingsPortLabel);

        // Build white-tinted clipboard icon
        QPixmap clipPix(16, 16);
        clipPix.fill(Qt::transparent);
        {
            QPainter clipPainter(&clipPix);
            QSvgRenderer clipRenderer(QStringLiteral(":/assets/clipboard2-plus.svg"));
            clipRenderer.render(&clipPainter);
            clipPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            clipPainter.fillRect(clipPix.rect(), Qt::white);
        }
        auto* portCopyBtn = new QPushButton(btnGroup);
        portCopyBtn->setObjectName(QStringLiteral("portCopyBtn"));
        portCopyBtn->setIcon(QIcon(clipPix));
        portCopyBtn->setIconSize({16, 16});
        portCopyBtn->setFixedSize(34, 30);
        portCopyBtn->setCursor(Qt::PointingHandCursor);
        portCopyBtn->setToolTip(tr("Copy to Clipboard"));
        connect(portCopyBtn, &QPushButton::clicked, this, [this]()
        {
            if (m_natPmpManager && m_natPmpManager->forwardedPort() > 0)
            {
                QApplication::clipboard()->setText(QString::number(m_natPmpManager->forwardedPort()));
            }
        });
        btnGroupLayout->addWidget(portCopyBtn);

        portRowLayout->addWidget(btnGroup, 0, Qt::AlignVCenter);
        portRowLayout->addStretch();
        plusLayout->addWidget(m_settingsPortRow);
        m_settingsPortRow->setVisible(false);

        // Wire NatPmpManager signals to update this display.
        if (m_natPmpManager != nullptr)
        {
            connect(m_natPmpManager, &NatPmpManager::portAcquired, this, [this](int port)
            {
                if (m_settingsPortLabel != nullptr)
                {
                    m_settingsPortLabel->setText(QString::number(port));
                }
                if (m_settingsPortRow != nullptr)
                {
                    m_settingsPortRow->setVisible(true);
                }
            });
            connect(m_natPmpManager, &NatPmpManager::portLost, this, [this]()
            {
                if (m_settingsPortRow != nullptr)
                {
                    m_settingsPortRow->setVisible(false);
                }
            });
            connect(m_natPmpManager, &NatPmpManager::natpmpcMissing, this, [this]()
            {
                if (m_settingsPortRow != nullptr)
                {
                    m_settingsPortRow->setVisible(false);
                }
            });
            // If a port is already active when settings is opened, show it immediately.
            if (m_natPmpManager->forwardedPort() > 0)
            {
                m_settingsPortLabel->setText(QString::number(m_natPmpManager->forwardedPort()));
                m_settingsPortRow->setVisible(true);
            }
        }
    }

    // ── Custom DNS ────────────────────────────────────────────
    {
        addDivider(plusLayout, m_plusSection);

        QWidget* dnsRow = new QWidget(m_plusSection);
        QHBoxLayout* dnsRl = new QHBoxLayout(dnsRow);
        dnsRl->setContentsMargins(16, 12, 16, 4);
        dnsRl->setSpacing(16);
        dnsRl->addWidget(makeTextCol(dnsRow,
                                     tr("Custom DNS"),
                                     tr("Override the VPN DNS with your own resolver(s). "
                                        "Separate multiple addresses with a comma.")), 1);
        m_dnsToggle = new ToggleWithStatus(dnsRow);
        dnsRl->addWidget(m_dnsToggle);
        plusLayout->addWidget(dnsRow);

        QWidget* dnsAddrRow = new QWidget(m_plusSection);
        dnsAddrRow->setVisible(false);
        QHBoxLayout* dnsAddrRl = new QHBoxLayout(dnsAddrRow);
        dnsAddrRl->setContentsMargins(16, 0, 16, 12);
        m_dnsEdit = new QLineEdit(dnsAddrRow);
        m_dnsEdit->setPlaceholderText(QStringLiteral("e.g. 1.1.1.1,8.8.8.8"));
        m_dnsEdit->setObjectName(QStringLiteral("settingsDnsEdit"));
        dnsAddrRl->addWidget(m_dnsEdit);
        m_dnsApplyBtn = new QPushButton(tr("Apply"), dnsAddrRow);
        m_dnsApplyBtn->setObjectName(QStringLiteral("secondaryButton"));
        m_dnsApplyBtn->setFixedHeight(28);
        dnsAddrRl->addWidget(m_dnsApplyBtn);
        plusLayout->addWidget(dnsAddrRow);

        connect(m_dnsToggle, &ToggleWithStatus::toggled, this, [this, dnsAddrRow](const bool on)
        {
            // Turning DNS off while connected requires a reconnect — show dialog
            // and revert the toggle until the user confirms.
            if (on == false && m_manager->currentState() != VpnState::Disconnected)
            {
                m_dnsToggle->blockSignals(true);
                m_dnsToggle->setOn(true, false); // revert — keep address row visible
                m_dnsToggle->blockSignals(false);

                showReconnectDialog(tr("Custom DNS"), [this, dnsAddrRow]()
                {
                    // Now that the user confirmed, hide the row and reconnect.
                    dnsAddrRow->setVisible(false);
                    m_dnsToggle->blockSignals(true);
                    m_dnsToggle->setOn(false, true);
                    m_dnsToggle->blockSignals(false);
                    m_sequencePending = true;
                    m_manager->applyConfigValueAndReconnect(QStringLiteral("custom-dns"), QStringLiteral("off"));
                });
                return;
            }

            dnsAddrRow->setVisible(on);
            if (on == false)
            {
                m_manager->applyConfigValue(QStringLiteral("custom-dns"), QStringLiteral("off"));
            }
        });

        connect(m_dnsApplyBtn, &QPushButton::clicked, this, [this]()
        {
            const QString dns = m_dnsEdit->text().trimmed();
            if (dns.isEmpty()) return;

            const QString cliValue = QStringLiteral("--dns %1 on").arg(dns);

            if (m_manager->currentState() != VpnState::Disconnected)
            {
                showReconnectDialog(tr("Custom DNS"), [this, cliValue]()
                {
                    m_sequencePending = true;
                    m_manager->applyConfigValueAndReconnect(
                        QStringLiteral("custom-dns"), cliValue);
                });
                return;
            }

            m_manager->applyConfigValue(QStringLiteral("custom-dns"), cliValue);
        });
    }

    vpnCardLayout->addStretch();

    // Apply the correct enabled/opacity state for the plus section right away
    // if the account type is already known (e.g. user was already logged in).
    updatePlusSectionState();

    // VpnManager signals
    connect(m_manager, &VpnManager::settingsReady,    this, &SettingsPage::onSettingsReady);
    connect(m_manager, &VpnManager::accountTypeReady, this, [this](AccountType)
    {
        updatePlusSectionState();
    });
    connect(m_manager, &VpnManager::cliVersionReady,  this, [this](const QString& v)
    {
        m_installedCliVersion = v;
    });
    connect(m_manager, &VpnManager::configApplied, this, &SettingsPage::maybeWarnReconnect);

    // Disable all VPN-tab settings while the VPN is connecting or disconnecting,
    // and keep them disabled throughout an applyConfigValueAndReconnect() sequence.
    connect(m_manager, &VpnManager::connectionStateChanged, this,
            [this](const VpnState state, const QString&)
    {
        const bool transitioning = state == VpnState::Connecting
                                || state == VpnState::Disconnecting;

        if (m_sequencePending && transitioning == false)
        {
            // We're in a change-and-reconnect sequence.
            // Keep the whole card locked through the Disconnected interim.
            if (state == VpnState::Connected || state == VpnState::Error)
            {
                // Sequence complete — re-read settings from disk so the toggles
                // show the confirmed on-disk state rather than the optimistic one.
                // refresh() will re-enable everything via setLoading(false) once
                // the settings are ready.
                m_sequencePending = false;
                refresh();
            }
            else
            {
                // Disconnected interim: re-disable what the card re-enable exposed.
                if (m_vpnCard != nullptr)    m_vpnCard->setEnabled(false);
                if (m_refreshBtn != nullptr) m_refreshBtn->setEnabled(false);
            }
            return;
        }

        // Normal (non-sequence) state change.
        if (m_vpnCard != nullptr)    m_vpnCard->setEnabled(!transitioning);
        if (m_refreshBtn != nullptr) m_refreshBtn->setEnabled(!transitioning);
        if (transitioning == false)
        {
            updatePlusSectionState();
        }
    });

    // Apply the transitioning state immediately in case the VPN is already
    // mid-transition when the settings page is first constructed.
    {
        const VpnState s = m_manager->currentState();
        const bool transitioning = s == VpnState::Connecting
                                || s == VpnState::Disconnecting;
        if (transitioning)
        {
            if (m_vpnCard != nullptr)    m_vpnCard->setEnabled(false);
            if (m_refreshBtn != nullptr) m_refreshBtn->setEnabled(false);
        }
    }

    // Spinner timer
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % kSpinnerFrameCount;
        m_statusLabel->setText(
            tr("%1 Loading settings\u2026").arg(QString::fromUtf8(kSpinnerFrames[m_spinnerFrame])));
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

void SettingsPage::updatePlusSectionState() const
{
    const bool isFree = m_manager->accountType() == AccountType::Free;

    // Shared helper: show/hide the divider, enable/disable the section,
    // and apply or remove a 45% opacity effect.
    auto applyToSection = [&](QWidget* section, QWidget* divider)
    {
        if (section == nullptr || divider == nullptr) return;
        divider->setVisible(isFree);
        section->setEnabled(!isFree);
        if (isFree)
        {
            auto* effect = qobject_cast<QGraphicsOpacityEffect*>(section->graphicsEffect());
            if (!effect)
            {
                effect = new QGraphicsOpacityEffect(section);
                section->setGraphicsEffect(effect);
            }
            effect->setOpacity(0.45);
        }
        else
        {
            section->setGraphicsEffect(nullptr);
        }
    };

    applyToSection(m_plusSection,    m_plusDivider);
    applyToSection(m_appPlusSection, m_appPlusDivider);
}

void SettingsPage::setLoading(const bool loading)
{
    m_loading = loading;
    m_refreshBtn->setEnabled(!loading);
    m_refreshBtn->setText(loading ? tr("Loading\u2026") : tr("↻ Refresh"));
    m_statusLabel->setVisible(loading);
    if (loading)
    {
        m_spinnerFrame = 0;
        m_statusLabel->setText(tr("\u28cb Loading settings\u2026"));
        m_spinnerTimer->start();
    }
    else
    {
        m_spinnerTimer->stop();
    }
    for (const ToggleRow& r : std::as_const(m_toggleRows))
    {
        r.toggle->setEnabled(!loading);
    }
    for (const ComboRow& r : std::as_const(m_comboRows))
    {
        r.combo->setEnabled(!loading);
    }

    const bool controlsEnabled = loading == false;
    if (m_killSwitchToggle != nullptr)
    {
        m_killSwitchToggle->setEnabled(controlsEnabled);
    }
    if (m_autoStartToggle != nullptr)
    {
        m_autoStartToggle->setEnabled(controlsEnabled);
    }
    if (m_notificationsToggle != nullptr)
    {
        m_notificationsToggle->setEnabled(controlsEnabled);
    }
    if (m_recentConnectionsSpinBox != nullptr)
    {
        m_recentConnectionsSpinBox->setEnabled(controlsEnabled);
    }
    if (m_dnsToggle != nullptr)
    {
        m_dnsToggle->setEnabled(controlsEnabled);
    }
    if (m_dnsApplyBtn != nullptr)
    {
        m_dnsApplyBtn->setEnabled(controlsEnabled);
    }
}

void SettingsPage::onSettingsReady(const QMap<QString, QString>& info)
{
    setLoading(false);

    auto val = [&](const QString& key)
    {
        return info.value(key).toLower().trimmed();
    };

    // Toggle rows – a row is ON if the value matches the row's onValue OR
    // one of the generic truthy strings (for standard "on"/"true" settings).
    for (const auto& row : std::as_const(m_toggleRows))
    {
        const QString v = val(row.cliKey);
        const bool on = (v == row.onValue) || isOnString(v);
        row.toggle->setOn(on, false);
    }

    // Kill switch – handled separately since it drives a collapsible sub-panel.
    // Block signals so the toggled handler (which guards against changes while
    // connected) cannot fire and accidentally revert the loaded state.
    if (m_killSwitchToggle != nullptr && m_killSwitchSubPanel != nullptr)
    {
        const bool ksOn = val(QStringLiteral("kill-switch")) == QLatin1String("standard");
        m_killSwitchToggle->blockSignals(true);
        m_killSwitchToggle->setOn(ksOn, false);
        m_killSwitchToggle->blockSignals(false);
        m_killSwitchSubPanel->setVisible(ksOn);
    }

    // Combo rows – find the matching CLI value and select that index
    for (const auto& row : std::as_const(m_comboRows))
    {
        const QString v = val(row.cliKey);
        int idx = row.cliValues.indexOf(v);
        idx = std::max(idx, 0);
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
    if (dnsOn)
    {
        m_dnsEdit->setText(dns);
    }
    else
    {
        m_dnsEdit->clear();
    }

    // Re-apply the plus section disabled state — setLoading(false) re-enables
    // individual widgets, so we must restore the correct state afterward.
    updatePlusSectionState();
}



