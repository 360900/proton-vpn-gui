#include "vpnpage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>

static QPixmap renderSvg(const QString &path, const QSize &size)
{
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    QSvgRenderer renderer(path);
    renderer.render(&p);
    return pix;
}

VpnPage::VpnPage(VpnManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(24);
    layout->setContentsMargins(40, 40, 40, 40);

    // State icon (big SVG)
    m_stateIconLabel = new QLabel(this);
    m_stateIconLabel->setAlignment(Qt::AlignCenter);
    m_stateIconLabel->setFixedSize(140, 140);
    layout->addWidget(m_stateIconLabel, 0, Qt::AlignCenter);

    // Status text
    m_statusLabel = new QLabel(QStringLiteral("Checking…"), this);
    m_statusLabel->setObjectName(QStringLiteral("vpnStatusLabel"));
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(20);
    statusFont.setBold(true);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    // Timer label
    m_timerLabel = new QLabel(this);
    m_timerLabel->setObjectName(QStringLiteral("timerLabel"));
    m_timerLabel->setAlignment(Qt::AlignCenter);
    m_timerLabel->setVisible(false);
    layout->addWidget(m_timerLabel, 0, Qt::AlignCenter);

    // Info label (IP / error details)
    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setWordWrap(true);
    layout->addWidget(m_infoLabel, 0, Qt::AlignCenter);

    // Connect / Disconnect button
    m_connectBtn = new QPushButton(this);
    m_connectBtn->setObjectName(QStringLiteral("connectButton"));
    m_connectBtn->setFixedSize(160, 48);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentState == VpnState::Connected) {
            emit disconnectRequested();
        } else {
            emit connectRequested();
        }
    });
    layout->addWidget(m_connectBtn, 0, Qt::AlignCenter);

    // Elapsed timer
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, [this]() {
        m_elapsedSeconds++;
        int h = m_elapsedSeconds / 3600;
        int m = (m_elapsedSeconds % 3600) / 60;
        int s = m_elapsedSeconds % 60;
        m_timerLabel->setText(QString::asprintf("%02d:%02d:%02d", h, m, s));
    });

    // Checking spinner — animates the status label while connection state is unknown
    static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static constexpr int frameCount = 10;
    m_checkingSpinnerTimer = new QTimer(this);
    m_checkingSpinnerTimer->setInterval(200);
    connect(m_checkingSpinnerTimer, &QTimer::timeout, this, [this]() {
        m_checkingSpinnerFrame = (m_checkingSpinnerFrame + 1) % frameCount;
        m_statusLabel->setText(
            QStringLiteral("%1 Checking…").arg(QString::fromUtf8(frames[m_checkingSpinnerFrame])));
    });

    // Start in Unknown — spinner runs until checkConnectionStatus responds
    updateUi(VpnState::Unknown, QString());
    m_checkingSpinnerTimer->start();
}

void VpnPage::onStateChanged(VpnState state, const QString &info)
{
    updateUi(state, info);
}

void VpnPage::updateUi(VpnState state, const QString &info)
{
    const VpnState prevState = m_currentState;
    m_currentState = state;
    const QSize iconSize(140, 140);

    if (state != VpnState::Unknown)
        m_checkingSpinnerTimer->stop();

    switch (state) {
    case VpnState::Connected:
        m_stateIconLabel->setPixmap(renderSvg(QStringLiteral(":/assets/state-connected.svg"), iconSize));
        m_statusLabel->setText(QStringLiteral("Connected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #1a9c5b;"));
        m_infoLabel->setText(info.isEmpty() ? QString() : info);
        m_connectBtn->setText(QStringLiteral("Disconnect"));
        m_connectBtn->setObjectName(QStringLiteral("disconnectButton"));
        m_connectBtn->setStyleSheet(QStringLiteral(
            "QPushButton#disconnectButton {"
            "  background-color: #d63f3f;"
            "  color: white;"
            "  border-radius: 6px;"
            "  font-weight: bold;"
            "  font-size: 14px;"
            "}"
            "QPushButton#disconnectButton:hover { background-color: #c03030; }"
        ));
        m_connectBtn->setEnabled(true);
        // Only start the elapsed timer if we know the connection just happened
        // (i.e. we were Connecting). If already connected on launch the elapsed
        // time is unknown, so hide the timer instead.
        if (prevState == VpnState::Connecting)
            startElapsedTimer();
        else
            stopElapsedTimer();
        break;

    case VpnState::Disconnected:
        m_stateIconLabel->setPixmap(renderSvg(QStringLiteral(":/assets/state-disconnected.svg"), iconSize));
        m_statusLabel->setText(QStringLiteral("Disconnected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #888888;"));
        m_infoLabel->setText(info.isEmpty() ? QString() : info);
        m_connectBtn->setText(QStringLiteral("Connect"));
        m_connectBtn->setObjectName(QStringLiteral("connectButton"));
        m_connectBtn->setStyleSheet(QStringLiteral(
            "QPushButton#connectButton {"
            "  background-color: #6d4aff;"
            "  color: white;"
            "  border-radius: 6px;"
            "  font-weight: bold;"
            "  font-size: 14px;"
            "}"
            "QPushButton#connectButton:hover { background-color: #5a3de0; }"
        ));
        m_connectBtn->setEnabled(true);
        stopElapsedTimer();
        break;

    case VpnState::Connecting:
        m_stateIconLabel->setPixmap(renderSvg(QStringLiteral(":/assets/state-disconnected.svg"), iconSize));
        m_statusLabel->setText(QStringLiteral("Connecting…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623;"));
        m_infoLabel->setText(QString());
        m_connectBtn->setText(QStringLiteral("Connecting…"));
        m_connectBtn->setEnabled(false);
        stopElapsedTimer();
        break;

    case VpnState::Disconnecting:
        m_stateIconLabel->setPixmap(renderSvg(QStringLiteral(":/assets/state-disconnected.svg"), iconSize));
        m_statusLabel->setText(QStringLiteral("Disconnecting…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623;"));
        m_connectBtn->setText(QStringLiteral("Disconnecting…"));
        m_connectBtn->setEnabled(false);
        stopElapsedTimer();
        break;

    case VpnState::Error:
        m_stateIconLabel->setPixmap(renderSvg(QStringLiteral(":/assets/state-error.svg"), iconSize));
        m_statusLabel->setText(QStringLiteral("Error"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #d63f3f;"));
        m_infoLabel->setText(info);
        m_connectBtn->setText(QStringLiteral("Try Again"));
        m_connectBtn->setEnabled(true);
        stopElapsedTimer();
        break;

    default:
        m_stateIconLabel->setPixmap(renderSvg(QStringLiteral(":/assets/state-disconnected.svg"), iconSize));
        m_statusLabel->setText(QStringLiteral("⠋ Checking…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #9999bb;"));
        m_infoLabel->setText(QString());
        m_connectBtn->setText(QStringLiteral("Connect"));
        m_connectBtn->setEnabled(false);
        stopElapsedTimer();
        break;
    }
}

void VpnPage::startElapsedTimer()
{
    m_elapsedSeconds = 0;
    m_timerLabel->setText(QStringLiteral("00:00:00"));
    m_timerLabel->setVisible(true);
    m_elapsedTimer->start();
}

void VpnPage::stopElapsedTimer()
{
    m_elapsedTimer->stop();
    m_timerLabel->setVisible(false);
}

