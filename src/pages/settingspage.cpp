#include "settingspage.h"
#include "../appconfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QScrollArea>
#include <QPainter>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

// ============================================================
// ToggleSwitch
// ============================================================

ToggleSwitch::ToggleSwitch(QWidget *parent) : QWidget(parent)
{
    setFixedSize(sizeHint());
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);
}

void ToggleSwitch::setOn(bool on, bool animate)
{
    if (m_on == on) return;
    m_on = on;
    if (animate) {
        m_anim->stop();
        m_anim->setStartValue(m_knobPos);
        m_anim->setEndValue(on ? 1.0 : 0.0);
        m_anim->start();
    } else {
        m_knobPos = on ? 1.0 : 0.0;
        update();
    }
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int w = width(), h = height(), r = h / 2;
    const QColor trackOn(0x6d, 0x4a, 0xff), trackOff(0x55, 0x55, 0x66);
    QColor track;
    track.setRed  (int(trackOff.red()   + (trackOn.red()   - trackOff.red())   * m_knobPos));
    track.setGreen(int(trackOff.green() + (trackOn.green() - trackOff.green()) * m_knobPos));
    track.setBlue (int(trackOff.blue()  + (trackOn.blue()  - trackOff.blue())  * m_knobPos));
    p.setBrush(track);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, w, h, r, r);
    const int knobD = h - 4, knobMin = 2, knobMax = w - knobD - 2;
    const int knobX = int(knobMin + (knobMax - knobMin) * m_knobPos);
    p.setBrush(Qt::white);
    p.drawEllipse(knobX, 2, knobD, knobD);
}

void ToggleSwitch::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) { setOn(!m_on); emit toggled(m_on); }
    QWidget::mousePressEvent(e);
}

// ============================================================
// SettingsPage helpers
// ============================================================

void SettingsPage::addDivider(QVBoxLayout *layout, QWidget *parent)
{
    auto *div = new QFrame(parent);
    div->setFrameShape(QFrame::HLine);
    div->setObjectName(QStringLiteral("divider"));
    layout->addWidget(div);
}

static QVBoxLayout *makeTextCol(QWidget *parent, const QString &label, const QString &desc)
{
    auto *col = new QVBoxLayout();
    auto *nameL = new QLabel(label, parent);
    nameL->setObjectName(QStringLiteral("infoKey"));
    col->addWidget(nameL);
    if (!desc.isEmpty()) {
        auto *descL = new QLabel(desc, parent);
        descL->setObjectName(QStringLiteral("settingsDesc"));
        descL->setWordWrap(true);
        QFont f = descL->font();
        f.setPointSize(qMax(f.pointSize() - 1, 7));
        descL->setFont(f);
        descL->setStyleSheet(QStringLiteral("color: #888;"));
        col->addWidget(descL);
    }
    return col;
}

void SettingsPage::maybeWarnReconnect(bool needsReconnect)
{
    if (!needsReconnect) return;
    if (m_manager->currentState() != VpnState::Connected) return;

    QMessageBox mb(this);
    mb.setWindowTitle(QStringLiteral("Reconnect Required"));
    mb.setIcon(QMessageBox::Information);
    mb.setText(QStringLiteral("This setting change will only take effect after reconnecting to the VPN."));
    mb.setStandardButtons(QMessageBox::Ok);
    mb.exec();
}

QWidget *SettingsPage::makeToggleRow(QWidget *parent, const QString &label,
                                     const QString &desc, const QString &cliKey,
                                     bool needsReconnect)
{
    auto *row = new QWidget(parent);
    auto *rl  = new QHBoxLayout(row);
    rl->setContentsMargins(16, 12, 16, 12);
    rl->addLayout(makeTextCol(row, label, desc));
    rl->addStretch();
    auto *toggle = new ToggleSwitch(row);
    rl->addWidget(toggle);

    connect(toggle, &ToggleSwitch::toggled, this, [this, cliKey, needsReconnect](bool on) {
        m_manager->applyConfigValue(cliKey, on ? QStringLiteral("on") : QStringLiteral("off"));
        maybeWarnReconnect(needsReconnect);
    });

    m_toggleRows.append({cliKey, toggle, needsReconnect});
    return row;
}

QWidget *SettingsPage::makeComboRow(QWidget *parent, const QString &label,
                                    const QString &desc, const QString &cliKey,
                                    const QStringList &labels, const QStringList &cliValues,
                                    bool needsReconnect)
{
    auto *row = new QWidget(parent);
    auto *rl  = new QHBoxLayout(row);
    rl->setContentsMargins(16, 12, 16, 12);
    rl->addLayout(makeTextCol(row, label, desc));
    rl->addStretch();
    auto *combo = new QComboBox(row);
    for (const auto &l : labels) combo->addItem(l);
    combo->setMinimumWidth(160);
    rl->addWidget(combo);

    connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, cliKey, cliValues, needsReconnect](int idx) {
        if (idx < 0 || idx >= cliValues.size()) return;
        m_manager->applyConfigValue(cliKey, cliValues[idx]);
        maybeWarnReconnect(needsReconnect);
    });

    m_comboRows.append({cliKey, combo, cliValues, needsReconnect});
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
    if (kDryRun)             return false; // dry-run: always report disabled
    if (!systemdAvailable()) return false;

    // Ask systemd whether the service is enabled. Exit code 0 = enabled.
    QProcess p;
    p.start(QStringLiteral("systemctl"),
            {QStringLiteral("--user"), QStringLiteral("is-enabled"), kServiceName});
    if (!p.waitForFinished(3000)) return false;
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed().toLower();
    return out == QStringLiteral("enabled") || out == QStringLiteral("static");
}

bool SettingsPage::setAutoStart(bool enable, QString &errorOut)
{
    if (kDryRun) {
        qDebug("[DryRun] setAutoStart(%s) — skipping real systemd operations.",
               enable ? "true" : "false");
        return true; // pretend success
    }

    if (enable) {
        // Resolve the path to the currently running executable.
        const QString exe = QCoreApplication::applicationFilePath();

        // Create the systemd user service directory if it doesn't exist.
        QDir dir;
        if (!dir.mkpath(kSystemdUserServiceDir)) {
            errorOut = QStringLiteral("Could not create directory: ") + kSystemdUserServiceDir;
            return false;
        }

        // Write the .service file.
        const QString serviceContent =
            QStringLiteral("[Unit]\n"
                           "Description=ProtonVPN Qt App\n"
                           "After=graphical-session.target\n"
                           "PartOf=graphical-session.target\n"
                           "\n"
                           "[Service]\n"
                           "Type=simple\n"
                           "ExecStart=%1\n"
                           "Restart=on-failure\n"
                           "Environment=DISPLAY=:0\n"
                           "\n"
                           "[Install]\n"
                           "WantedBy=graphical-session.target\n").arg(exe);

        QFile f(serviceFilePath());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
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
        if (p.exitCode() != 0) {
            errorOut = QString::fromUtf8(p.readAllStandardError()).trimmed();
            if (errorOut.isEmpty())
                errorOut = QStringLiteral("systemctl --user enable failed (exit %1)").arg(p.exitCode());
            return false;
        }
    } else {
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

void SettingsPage::updateAutoConnectRowVisibility()
{
    if (!m_autoConnectRow) return;
    const bool show = m_autoStartToggle && m_autoStartToggle->isOn();
    m_autoConnectRow->setVisible(show);
    // If auto-start is turned off, also disable auto-connect and persist that.
    if (!show && m_autoConnectToggle && m_autoConnectToggle->isOn()) {
        m_autoConnectToggle->setOn(false, false);
        AppConfig::instance().setAutoConnect(false);
    }
}

SettingsPage::SettingsPage(VpnManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(16, 16, 16, 16);
    outerLayout->setSpacing(12);

    // Header
    auto *headerRow = new QHBoxLayout();
    auto *titleLabel = new QLabel(QStringLiteral("Settings"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();
    m_refreshBtn = new QPushButton(QStringLiteral("↻ Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(30);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SettingsPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    outerLayout->addLayout(headerRow);

    // Spinner/status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setObjectName(QStringLiteral("settingsStatusLabel"));
    m_statusLabel->setVisible(false);
    outerLayout->addWidget(m_statusLabel);

    // Scrollable card
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *card = new QWidget();
    card->setObjectName(QStringLiteral("infoCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    bool first = true;
    auto add = [&](QWidget *w) {
        if (!first) addDivider(cardLayout, card);
        first = false;
        cardLayout->addWidget(w);
    };

    // ── Auto-start (systemd user service) ────────────────────
    // Only show this row if systemd is available on this system.
    if (systemdAvailable()) {
        m_autoStartRow = new QWidget(card);
        auto *rl = new QHBoxLayout(m_autoStartRow);
        rl->setContentsMargins(16, 12, 16, 12);
        rl->addLayout(makeTextCol(m_autoStartRow,
            QStringLiteral("Launch on Startup"),
            QStringLiteral("Automatically start the app in the background when you log in "
                           "(installs a systemd user service).")));
        rl->addStretch();
        m_autoStartToggle = new ToggleSwitch(m_autoStartRow);
        m_autoStartToggle->setOn(autoStartEnabled(), false);
        connect(m_autoStartToggle, &ToggleSwitch::toggled, this, [this](bool on) {
            QString err;
            if (!setAutoStart(on, err)) {
                m_autoStartToggle->setOn(!on, false);
                QMessageBox::warning(this,
                    QStringLiteral("Auto-start Error"),
                    QStringLiteral("Failed to %1 auto-start:\n%2")
                        .arg(on ? QStringLiteral("enable") : QStringLiteral("disable"), err));
            } else {
                updateAutoConnectRowVisibility();
            }
        });
        rl->addWidget(m_autoStartToggle);
        add(m_autoStartRow);

        // ── Auto-connect on startup ───────────────────────────
        // Shown indented beneath auto-start, only when auto-start is on.
        m_autoConnectRow = new QWidget(card);
        auto *acRl = new QHBoxLayout(m_autoConnectRow);
        acRl->setContentsMargins(32, 8, 16, 12); // extra left indent
        acRl->addLayout(makeTextCol(m_autoConnectRow,
            QStringLiteral("Auto-connect on Startup"),
            QStringLiteral("Automatically connect to the VPN when the app starts.")));
        acRl->addStretch();
        m_autoConnectToggle = new ToggleSwitch(m_autoConnectRow);
        m_autoConnectToggle->setOn(AppConfig::instance().autoConnect(), false);
        connect(m_autoConnectToggle, &ToggleSwitch::toggled, this, [](bool on) {
            AppConfig::instance().setAutoConnect(on);
        });
        acRl->addWidget(m_autoConnectToggle);
        cardLayout->addWidget(m_autoConnectRow); // added directly, not via add() — shares divider with autostart

        updateAutoConnectRowVisibility();
    }

    // ── Anonymous Crash Reports (on/off) ──────────────────────
    add(makeToggleRow(card,
        QStringLiteral("Anonymous Crash Reports"),
        QStringLiteral("Send anonymous crash reports to Proton for the VPN CLI tool — not this Qt app."),
        QStringLiteral("anonymous-crash-reports"), false));

    // ── IPv6 (on/off, reconnect) ──────────────────────────────
    add(makeToggleRow(card,
        QStringLiteral("IPv6"),
        QStringLiteral("Enable IPv6 support over the VPN tunnel."),
        QStringLiteral("ipv6"), true));

    // ── Moderate NAT (on/off, reconnect) ─────────────────────
    add(makeToggleRow(card,
        QStringLiteral("Moderate NAT"),
        QStringLiteral("Use NAT Type 2 for better compatibility with games and P2P."),
        QStringLiteral("moderate-nat"), true));

    // ── Kill Switch (off / standard / full) ──────────────────
    add(makeComboRow(card,
        QStringLiteral("Kill Switch"),
        QStringLiteral("Block traffic if the VPN connection drops. "
                       "\"Standard\" only blocks while reconnecting; "
                       "\"Permanent\" blocks even when the VPN is off."),
        QStringLiteral("kill-switch"),
        {QStringLiteral("Off"), QStringLiteral("Standard"), QStringLiteral("Permanent")},
        {QStringLiteral("off"), QStringLiteral("standard"), QStringLiteral("full")},
        false));

    // ── VPN Accelerator (on/off, reconnect) ──────────────────
    add(makeToggleRow(card,
        QStringLiteral("VPN Accelerator"),
        QStringLiteral("Boost connection speeds using advanced protocol techniques."),
        QStringLiteral("vpn-accelerator"), true));

    // ── NetShield (off / malware / malware+ads) ───────────────
    add(makeComboRow(card,
        QStringLiteral("NetShield Ad-blocker"),
        QStringLiteral("Block malware, ads, and trackers at the DNS level."),
        QStringLiteral("netshield"),
        {QStringLiteral("Off"),
         QStringLiteral("Malware only"),
         QStringLiteral("Malware, ads & trackers")},
        {QStringLiteral("off"),
         QStringLiteral("malware-only"),
         QStringLiteral("malware-ads-trackers")},
        true));

    // ── Port Forwarding (on/off, reconnect) ──────────────────
    add(makeToggleRow(card,
        QStringLiteral("Port Forwarding"),
        QStringLiteral("Allow incoming connections through the VPN. "
                       "Reconnect after enabling for changes to take effect."),
        QStringLiteral("port-forwarding"), true));

    // ── Custom DNS ────────────────────────────────────────────
    {
        addDivider(cardLayout, card);

        // Top sub-row: label + toggle
        auto *dnsRow = new QWidget(card);
        auto *dnsRl  = new QHBoxLayout(dnsRow);
        dnsRl->setContentsMargins(16, 12, 16, 4);
        dnsRl->addLayout(makeTextCol(dnsRow,
            QStringLiteral("Custom DNS"),
            QStringLiteral("Override the VPN DNS with your own resolver(s). "
                           "Separate multiple addresses with a comma.")));
        dnsRl->addStretch();
        m_dnsToggle = new ToggleSwitch(dnsRow);
        dnsRl->addWidget(m_dnsToggle);
        cardLayout->addWidget(dnsRow);

        // Bottom sub-row: address field + Apply (only visible when toggle is on)
        auto *dnsAddrRow = new QWidget(card);
        dnsAddrRow->setVisible(false);
        auto *dnsAddrRl  = new QHBoxLayout(dnsAddrRow);
        dnsAddrRl->setContentsMargins(16, 0, 16, 12);
        m_dnsEdit = new QLineEdit(dnsAddrRow);
        m_dnsEdit->setPlaceholderText(QStringLiteral("e.g. 1.1.1.1,8.8.8.8"));
        m_dnsEdit->setObjectName(QStringLiteral("settingsDnsEdit"));
        dnsAddrRl->addWidget(m_dnsEdit);
        m_dnsApplyBtn = new QPushButton(QStringLiteral("Apply"), dnsAddrRow);
        m_dnsApplyBtn->setObjectName(QStringLiteral("secondaryButton"));
        m_dnsApplyBtn->setFixedHeight(28);
        dnsAddrRl->addWidget(m_dnsApplyBtn);
        cardLayout->addWidget(dnsAddrRow);

        // Wiring
        connect(m_dnsToggle, &ToggleSwitch::toggled, this, [this, dnsAddrRow](bool on) {
            dnsAddrRow->setVisible(on);
            if (!on) {
                m_manager->applyConfigValue(QStringLiteral("custom-dns"), QStringLiteral("off"));
                maybeWarnReconnect(true);
            }
        });
        connect(m_dnsApplyBtn, &QPushButton::clicked, this, [this]() {
            const QString dns = m_dnsEdit->text().trimmed();
            if (dns.isEmpty()) return;
            m_manager->applyConfigValue(
                QStringLiteral("custom-dns"),
                QStringLiteral("--dns %1 on").arg(dns));
            maybeWarnReconnect(true);
        });
    }

    cardLayout->addStretch();
    scrollArea->setWidget(card);
    outerLayout->addWidget(scrollArea, 1);

    // About button
    auto *aboutBtn = new QPushButton(QStringLiteral("About"), this);
    aboutBtn->setObjectName(QStringLiteral("secondaryButton"));
    aboutBtn->setCursor(Qt::PointingHandCursor);
    connect(aboutBtn, &QPushButton::clicked, this, &SettingsPage::showAboutDialog);
    outerLayout->addWidget(aboutBtn);

    // VpnManager signal
    connect(m_manager, &VpnManager::settingsReady, this, &SettingsPage::onSettingsReady);

    // Spinner timer
    static const char *const frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    static constexpr int frameCount = 10;
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this, frames]() {
        m_spinnerFrame = (m_spinnerFrame + 1) % frameCount;
        m_statusLabel->setText(
            QStringLiteral("%1 Loading settings…").arg(QString::fromUtf8(frames[m_spinnerFrame])));
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

void SettingsPage::setLoading(bool loading)
{
    m_loading = loading;
    m_refreshBtn->setEnabled(!loading);
    m_refreshBtn->setText(loading ? QStringLiteral("Loading…") : QStringLiteral("↻ Refresh"));
    m_statusLabel->setVisible(loading);
    if (loading) {
        m_spinnerFrame = 0;
        m_statusLabel->setText(QStringLiteral("⠋ Loading settings…"));
        m_spinnerTimer->start();
    } else {
        m_spinnerTimer->stop();
    }
    for (const auto &r : std::as_const(m_toggleRows)) r.toggle->setEnabled(!loading);
    for (const auto &r : std::as_const(m_comboRows))  r.combo->setEnabled(!loading);
    if (m_autoStartToggle) m_autoStartToggle->setEnabled(!loading);
    if (m_dnsToggle)       m_dnsToggle->setEnabled(!loading);
    if (m_dnsApplyBtn)     m_dnsApplyBtn->setEnabled(!loading);
}

void SettingsPage::onSettingsReady(const QMap<QString, QString> &info)
{
    setLoading(false);

    auto val = [&](const QString &key) {
        return info.value(key).toLower().trimmed();
    };
    auto isOn = [&](const QString &key) {
        const QString v = val(key);
        return v == QLatin1String("on") || v == QLatin1String("true")
            || v == QLatin1String("1") || v == QLatin1String("enabled");
    };

    // Toggle rows
    for (const auto &row : std::as_const(m_toggleRows))
        row.toggle->setOn(isOn(row.cliKey), false);

    // Combo rows – find the matching CLI value and select that index
    for (const auto &row : std::as_const(m_comboRows)) {
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
    const bool dnsOn  = !dns.isEmpty()
                     && dns.toLower() != QLatin1String("disabled")
                     && dns.toLower() != QLatin1String("off")
                     && dns.toLower() != QLatin1String("none");
    m_dnsToggle->setOn(dnsOn, false);
    if (dnsOn) m_dnsEdit->setText(dns);
    else       m_dnsEdit->clear();
}

void SettingsPage::showAboutDialog()
{
    // Load versions from the embedded version.json resource
    QString appVersion = QStringLiteral("unknown");
    QString cliVersion = QStringLiteral("unknown");
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly)) {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        if (obj.contains(QStringLiteral("app_version")))
            appVersion = obj[QStringLiteral("app_version")].toString();
        if (obj.contains(QStringLiteral("cli_version_tested")))
            cliVersion = obj[QStringLiteral("cli_version_tested")].toString();
    }

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("About ProtonVPN Qt App"));
    dlg->setMinimumSize(520, 400);

    auto *layout = new QVBoxLayout(dlg);
    layout->setSpacing(16);

    auto *browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHtml(QStringLiteral(R"(
<h2 style="margin-bottom:4px;">ProtonVPN Qt App</h2>
<p style="color:#888;margin-top:0;">A community-built Qt 6 front-end for the Proton VPN CLI.</p>
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
</ul>
<hr/>
<p style="color:#888;font-size:small;">
  This software is provided as-is, without warranty of any kind. Use at your own risk.
</p>
)").arg(appVersion, cliVersion));
    layout->addWidget(browser);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
    connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
    layout->addWidget(btns);

    dlg->exec();
    dlg->deleteLater();
}

