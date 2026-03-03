#include "vpnpage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QPropertyAnimation>
#include <QEnterEvent>
#include <QDialog>
#include <QPlainTextEdit>
#include <QFont>
#include <QGuiApplication>
#include <QClipboard>
#include <cmath>

// ============================================================
// PowerButton implementation
// ============================================================

static constexpr int BTN_SIZE = 160; // outer widget size (px)
static constexpr int RING_WIDTH = 7; // ring stroke width
static constexpr int ICON_SIZE = 80; // power SVG render size

PowerButton::PowerButton(QWidget* parent) : QWidget(parent)
{
    setFixedSize(BTN_SIZE, BTN_SIZE);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_TranslucentBackground);

    // Clip the widget to a circle so the background/hover area isn't a square
    const QRegion mask(0, 0, BTN_SIZE, BTN_SIZE, QRegion::Ellipse);
    setMask(mask);

    m_anim = new QPropertyAnimation(this, "spinAngle", this);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(360.0);
    m_anim->setDuration(900);
    m_anim->setLoopCount(-1); // infinite
}

void PowerButton::setState(const RingState s)
{
    if (m_state == s) return;
    m_state = s;
    if (s == RingState::Spinning)
        startSpin();
    else
        stopSpin();
    update();
}

void PowerButton::startSpin() const
{
    if (m_anim->state() != QAbstractAnimation::Running)
        m_anim->start();
}

void PowerButton::stopSpin()
{
    m_anim->stop();
    m_spinAngle = 0.0;
}

void PowerButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF widgetRect = rect();
    constexpr qreal margin = RING_WIDTH / 2.0 + 4.0;
    const QRectF ringRect = widgetRect.adjusted(margin, margin, -margin, -margin);

    // ── ring / arc ──────────────────────────────────────────
    QPen ringPen;
    ringPen.setWidth(RING_WIDTH);
    ringPen.setCapStyle(Qt::RoundCap);

    if (m_state == RingState::Spinning)
    {
        // Draw a 270° arc that rotates
        QColor arcColor(0xa0, 0xa0, 0xa0);
        ringPen.setColor(arcColor);
        p.setPen(ringPen);
        // Qt angles: 0 = 3 o'clock, counter-clockwise positive
        // We want clockwise animation, so use negative span
        const int startAngle = static_cast<int>((90.0 - m_spinAngle) * 16.0); // top → rotates cw
        constexpr int spanAngle = -270 * 16;
        p.drawArc(ringRect, startAngle, spanAngle);
    }
    else
    {
        QColor ringColor;
        if (m_state == RingState::Connected)
            ringColor = QColor(0x1a, 0x9c, 0x5b); // green
        else if (m_state == RingState::Disconnected)
            ringColor = QColor(0xd6, 0x3f, 0x3f); // red
        else
            ringColor = QColor(0x55, 0x55, 0x77); // unknown – dim purple

        ringPen.setColor(ringColor);
        p.setPen(ringPen);
        p.drawEllipse(ringRect);
    }

    // ── hover glow ──────────────────────────────────────────
    if (m_hovered)
    {
        QColor glow(0xff, 0xff, 0xff, 18);
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(widgetRect.adjusted(margin + RING_WIDTH / 2,
                                          margin + RING_WIDTH / 2,
                                          -(margin + RING_WIDTH / 2),
                                          -(margin + RING_WIDTH / 2)));
    }

    // ── power SVG ───────────────────────────────────────────
    constexpr QRectF iconRect(
        (BTN_SIZE - ICON_SIZE) / 2.0,
        (BTN_SIZE - ICON_SIZE) / 2.0,
        ICON_SIZE,
        ICON_SIZE
    );

    // Render SVG into a pixmap, then tint it white when in dark mode
    const bool darkMode = palette().color(QPalette::Window).lightness() < 128;
    QPixmap iconPix(ICON_SIZE, ICON_SIZE);
    iconPix.fill(Qt::transparent);
    {
        QPainter ip(&iconPix);
        QSvgRenderer renderer(QStringLiteral(":/assets/power.svg"));
        renderer.render(&ip);
        if (darkMode)
        {
            ip.setCompositionMode(QPainter::CompositionMode_SourceIn);
            ip.fillRect(iconPix.rect(), Qt::white);
        }
    }
    p.drawPixmap(iconRect.toRect(), iconPix);
}

void PowerButton::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        emit clicked();
    QWidget::mousePressEvent(e);
}

void PowerButton::enterEvent(QEnterEvent* e)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(e);
}

void PowerButton::leaveEvent(QEvent* e)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(e);
}

// ============================================================
// VpnPage implementation
// ============================================================
// SvgBanner – responsive SVG widget that maintains a fixed aspect ratio
// and always fills its parent's width.
// ============================================================

class SvgBanner : public QWidget
{
public:
    // aspectRatio = width / height  (e.g. 4.0 for a 4:1 banner)
    explicit SvgBanner(const QString& resource, qreal aspectRatio, QWidget* parent = nullptr)
        : QWidget(parent), m_renderer(resource), m_aspect(aspectRatio)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMaximumWidth(1000);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int w = qMin(parentWidget() ? parentWidget()->width() : 320, 1000);
        return {w, qRound(w / m_aspect)};
    }

    [[nodiscard]] int heightForWidth(const int w) const override { return qRound(w / m_aspect); }
    [[nodiscard]] bool hasHeightForWidth() const override { return true; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        m_renderer.render(&p, QRectF(rect()));
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        // Keep height in sync with width
        const int h = qRound(width() / m_aspect);
        if (height() != h)
            setFixedHeight(h);
    }

private:
    QSvgRenderer m_renderer;
    qreal m_aspect;
};

// ============================================================

VpnPage::VpnPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager)
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(24);
    layout->setContentsMargins(40, 40, 40, 40);

    // Proton VPN logo banner – pinned to top, fills available width at 4:1 aspect ratio (max 1000px, centered)
    auto* logoWidget = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), 4.0, this);
    auto* logoRow = new QHBoxLayout();
    logoRow->setContentsMargins(0, 0, 0, 0);
    logoRow->addStretch(1);
    logoRow->addWidget(logoWidget);
    logoRow->addStretch(1);
    layout->addLayout(logoRow);

    // Push the rest of the content to vertical centre
    layout->addStretch(1);

    // Power button
    m_powerBtn = new PowerButton(this);
    connect(m_powerBtn, &PowerButton::clicked, this, [this]()
    {
        if (m_currentState == VpnState::Connected)
            emit disconnectRequested();
        else if (m_currentState == VpnState::Disconnected || m_currentState == VpnState::Error)
            emit connectRequested();
    });
    layout->addWidget(m_powerBtn, 0, Qt::AlignCenter);

    // Status text
    m_statusLabel = new QLabel(QStringLiteral("Checking…"), this);
    m_statusLabel->setObjectName(QStringLiteral("vpnStatusLabel"));
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
    m_infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_infoLabel);

    // "View Details" button – shown only on error
    m_errorDetailsBtn = new QPushButton(QStringLiteral("View Details"), this);
    m_errorDetailsBtn->setVisible(false);
    m_errorDetailsBtn->setFixedWidth(140);
    connect(m_errorDetailsBtn, &QPushButton::clicked, this, &VpnPage::showErrorDetails);
    layout->addWidget(m_errorDetailsBtn, 0, Qt::AlignCenter);

    // Balance the stretch so the content block sits in the middle of the remaining space
    layout->addStretch(1);

    // Elapsed timer
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, [this]()
    {
        m_elapsedSeconds++;
        int h = m_elapsedSeconds / 3600;
        int m = (m_elapsedSeconds % 3600) / 60;
        int s = m_elapsedSeconds % 60;
        m_timerLabel->setText(QString::asprintf("%02d:%02d:%02d", h, m, s));
    });

    // Checking spinner — animates the status label while connection state is unknown
    static const char* const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static constexpr int frameCount = 10;
    m_checkingSpinnerTimer = new QTimer(this);
    m_checkingSpinnerTimer->setInterval(200);
    connect(m_checkingSpinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_checkingSpinnerFrame = (m_checkingSpinnerFrame + 1) % frameCount;
        m_statusLabel->setText(
            QStringLiteral("%1 Checking…").arg(QString::fromUtf8(frames[m_checkingSpinnerFrame])));
    });

    // Start in Unknown — spinner runs until checkConnectionStatus responds
    updateUi(VpnState::Unknown, QString());
    m_checkingSpinnerTimer->start();
}

void VpnPage::onStateChanged(VpnState state, const QString& info)
{
    updateUi(state, info);
}

void VpnPage::updateUi(const VpnState state, const QString& info)
{
    const VpnState prevState = m_currentState;
    m_currentState = state;

    if (state != VpnState::Unknown)
        m_checkingSpinnerTimer->stop();

    // Hide the error details button by default; the Error case will re-show it
    m_errorDetailsBtn->setVisible(false);
    m_infoLabel->setTextFormat(Qt::AutoText);
    m_infoLabel->setOpenExternalLinks(false);

    switch (state)
    {
    case VpnState::Connected:
        m_powerBtn->setState(PowerButton::RingState::Connected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("Connected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #1a9c5b; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(info.isEmpty() ? QString() : info);
        if (prevState == VpnState::Connecting)
            startElapsedTimer();
        else
            stopElapsedTimer();
        break;

    case VpnState::Disconnected:
        m_powerBtn->setState(PowerButton::RingState::Disconnected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("Disconnected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(info.isEmpty() ? QString() : info);
        stopElapsedTimer();
        break;

    case VpnState::Connecting:
        m_powerBtn->setState(PowerButton::RingState::Spinning);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Connecting…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(QString());
        stopElapsedTimer();
        break;

    case VpnState::Disconnecting:
        m_powerBtn->setState(PowerButton::RingState::Spinning);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Disconnecting…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        stopElapsedTimer();
        break;

    case VpnState::Error:
    {
        m_powerBtn->setState(PowerButton::RingState::Disconnected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("Error"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #d63f3f; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        stopElapsedTimer();

        m_rawError = info;

        // Distinguish CLI errors (Python traceback) from app errors
        const bool isCliError = info.contains(QLatin1String("Traceback (most recent call last)"))
                             || info.contains(QLatin1String("File \"/usr/bin/protonvpn\""))
                             || info.contains(QLatin1String("File \"/usr/lib/python"));
        if (isCliError)
        {
            m_infoLabel->setText(QStringLiteral(
                "An error occurred in the Proton VPN CLI.\n"
                "Please file a bug report at "
                "<a href='https://github.com/ProtonVPN/proton-vpn-cli/issues'>github.com/ProtonVPN/proton-vpn-cli/</a>."));
        }
        else
        {
            m_infoLabel->setText(QStringLiteral(
                "An error occurred in the ProtonVPN Qt desktop app.\n"
                "Please file a bug report at "
                "<a href='https://github.com/wheat32/proton-vpn-qt-app/issues'>github.com/wheat32/proton-vpn-qt-app</a>."));
        }
        m_infoLabel->setTextFormat(Qt::RichText);
        m_infoLabel->setOpenExternalLinks(true);
        m_errorDetailsBtn->setVisible(!info.trimmed().isEmpty());
        break;
    }

    default: // Unknown
        m_powerBtn->setState(PowerButton::RingState::Unknown);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("⠋ Checking…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #9999bb; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(QString());
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

void VpnPage::stopElapsedTimer() const
{
    m_elapsedTimer->stop();
    m_timerLabel->setVisible(false);
}

void VpnPage::showErrorDetails() const
{
    auto* dlg = new QDialog(const_cast<VpnPage*>(this));
    dlg->setWindowTitle(QStringLiteral("Error Details"));
    dlg->setMinimumSize(640, 400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(10);

    auto* textEdit = new QPlainTextEdit(dlg);
    textEdit->setReadOnly(true);
    textEdit->setPlainText(m_rawError);
    textEdit->setFont(QFont(QStringLiteral("Monospace"), 9));
    layout->addWidget(textEdit);

    auto* btnRow = new QHBoxLayout();
    auto* copyBtn = new QPushButton(QStringLiteral("Copy to Clipboard"), dlg);
    connect(copyBtn, &QPushButton::clicked, dlg, [this]()
    {
        QGuiApplication::clipboard()->setText(m_rawError);
    });
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dlg->exec();
}

