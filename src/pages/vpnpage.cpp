#include "vpnpage.h"
#include "../geoutils.h"

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
#include <QScrollArea>
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
        const int w = qMin(width() > 0 ? width() : (parentWidget() ? parentWidget()->width() : 320), 1000);
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
// LocationPicker implementation
// ============================================================

// Feature metadata used to render per-item icons in the popup list
struct FeatureMeta { QString keyword; QString resource; QString tooltip; };
static const FeatureMeta kLocationFeatures[] = {
    { QStringLiteral("p2p"),         QStringLiteral(":/assets/server-p2p.svg"),
      QStringLiteral("P2P — Optimized for peer-to-peer file sharing") },
    { QStringLiteral("secure core"), QStringLiteral(":/assets/server-secure-core.svg"),
      QStringLiteral("Secure Core — Routes traffic through privacy-friendly countries") },
    { QStringLiteral("tor"),         QStringLiteral(":/assets/server-tor.svg"),
      QStringLiteral("Tor — Routes traffic through the Tor anonymity network") },
};

LocationPicker::LocationPicker(const QString& countryCode, QWidget* parent)
    : QFrame(parent), m_countryCode(countryCode)
{
    setObjectName(QStringLiteral("locationPicker"));
    setFixedWidth(260);

    // ── Header row (always visible, acts as the button) ──────────────────
    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("locationPickerHeader"));
    header->setCursor(Qt::PointingHandCursor);

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(10, 8, 10, 8);
    headerLayout->setSpacing(10);

    // Flag
    m_flagLabel = new QLabel(header);
    m_flagLabel->setFixedSize(28, 21);
    m_flagLabel->setAlignment(Qt::AlignCenter);
    if (!countryCode.isEmpty())
    {
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + countryCode.toLower(), 28);
        if (!pm.isNull())
            m_flagLabel->setPixmap(pm);
    }
    headerLayout->addWidget(m_flagLabel);

    // Two-line text block
    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    textCol->setContentsMargins(0, 0, 0, 0);

    m_topLine = new QLabel(QStringLiteral("Selected Location"), header);
    m_topLine->setObjectName(QStringLiteral("locationPickerTop"));

    m_bottomLine = new QLabel(QStringLiteral("⚡  Fastest server"), header);
    m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));

    textCol->addWidget(m_topLine);
    textCol->addWidget(m_bottomLine);
    headerLayout->addLayout(textCol);
    headerLayout->addStretch();

    // Chevron
    m_chevron = new QLabel(QStringLiteral("▾"), header);
    m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
    headerLayout->addWidget(m_chevron);

    // ── Outer layout — header only, popup is a floating window ───────────
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);

    // ── Popup — top-level frameless Qt::Popup window ─────────────────────
    // Qt::Popup gives us: floats above everything, auto-closes on outside click,
    // no taskbar entry, no frame. Exactly like QComboBox's internal drop-down.
    m_popup = new QFrame(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_popup->setObjectName(QStringLiteral("locationPickerPopup"));
    m_popup->setAttribute(Qt::WA_TranslucentBackground, false);

    auto* popupLayout = new QVBoxLayout(m_popup);
    popupLayout->setContentsMargins(0, 0, 0, 0);
    popupLayout->setSpacing(0);

    m_list = new QListWidget(m_popup);
    m_list->setObjectName(QStringLiteral("locationPickerList"));
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    popupLayout->addWidget(m_list);

    // Header click toggles popup; Qt::Popup handles outside-click dismissal.
    // Reset the chevron whenever the popup closes for any reason.
    header->installEventFilter(this);
    connect(m_list, &QListWidget::itemClicked, this, &LocationPicker::onItemClicked);
    m_popup->installEventFilter(this);

    // Start in loading state immediately — visible but not yet populated
    setLoading(true);
}

bool LocationPicker::eventFilter(QObject* obj, QEvent* ev)
{
    // Popup hidden by Qt (outside click auto-dismiss) → reset chevron
    if (obj == m_popup && ev->type() == QEvent::Hide)
    {
        m_chevron->setText(QStringLiteral("▾"));
        return false;
    }

    // Header click → toggle
    if (obj->isWidgetType() && ev->type() == QEvent::MouseButtonRelease)
    {
        auto* w = static_cast<QWidget*>(obj);
        if (w->objectName() == QLatin1String("locationPickerHeader"))
        {
            QWidget* p = w;
            while (p) { if (p == this) { togglePopup(); return true; } p = p->parentWidget(); }
        }
    }
    return QFrame::eventFilter(obj, ev);
}

void LocationPicker::togglePopup()
{
    if (m_list->count() == 0) return; // don't open while still loading

    if (m_popup->isVisible())
    {
        closePopup();
        return;
    }

    // Size the list to show all items (up to 8 rows) before showing
    resizeList();

    // Position the popup flush below the header, aligned to our left edge
    const QPoint globalBottomLeft = mapToGlobal(QPoint(0, height()));
    m_popup->setFixedWidth(width());
    m_popup->move(globalBottomLeft);
    m_popup->show();
    m_chevron->setText(QStringLiteral("▴"));
}

void LocationPicker::closePopup()
{
    m_popup->hide();
    m_chevron->setText(QStringLiteral("▾"));
}

void LocationPicker::resizeList()
{
    const int count = m_list->count();
    if (count == 0) return;
    const int rowH = m_list->sizeHintForRow(0);
    const int rows = qMin(count, 8);
    m_list->setFixedHeight(rows * rowH + 2);
    m_popup->adjustSize();
}

void LocationPicker::onItemClicked(QListWidgetItem* item)
{
    const QString clicked = item->data(Qt::UserRole).toString();
    closePopup();

    if (clicked == m_selectedCity)
        return; // same selection — no signal, no confirmation dialog

    m_selectedCity = clicked;
    updateHeader();
    emit selectionChanged(m_selectedCity);
}

void LocationPicker::updateHeader()
{
    if (m_selectedCity.isEmpty())
        m_bottomLine->setText(QStringLiteral("⚡  Fastest server"));
    else
        m_bottomLine->setText(m_selectedCity);
}

void LocationPicker::setLoading(bool loading)
{
    if (loading)
    {
        // Animate the bottom line with braille spinner frames
        static const char* const frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        if (!m_loadingTimer)
        {
            m_loadingTimer = new QTimer(this);
            m_loadingTimer->setInterval(120);
            connect(m_loadingTimer, &QTimer::timeout, this, [this]()
            {
                m_loadingFrame = (m_loadingFrame + 1) % 10;
                static const char* const fr[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
                m_bottomLine->setText(
                    QStringLiteral("%1 Loading locations…")
                        .arg(QString::fromUtf8(fr[m_loadingFrame])));
            });
        }
        m_bottomLine->setText(
            QStringLiteral("%1 Loading locations…")
                .arg(QString::fromUtf8(frames[0])));
        m_loadingTimer->start();
        m_chevron->setVisible(false);
        setVisible(true);
    }
    else
    {
        if (m_loadingTimer) m_loadingTimer->stop();
        m_chevron->setVisible(true);
        updateHeader();
    }
}

void LocationPicker::populate(const QList<QPair<QString, QString>>& cities)
{
    setLoading(false);
    m_list->clear();

    // ── Fastest server entry ──────────────────────────────────────────────
    auto* fastestItem = new QListWidgetItem();
    fastestItem->setData(Qt::UserRole, QString());

    auto* fastestRow = new QWidget();
    auto* fbox = new QHBoxLayout(fastestRow);
    fbox->setContentsMargins(10, 6, 10, 6);
    fbox->setSpacing(8);

    auto* fIcon = new QLabel(fastestRow);
    if (!m_countryCode.isEmpty())
    {
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + m_countryCode.toLower(), 20);
        if (!pm.isNull()) { fIcon->setPixmap(pm); fIcon->setFixedSize(24, 18); }
    }
    fbox->addWidget(fIcon, 0, Qt::AlignVCenter);

    auto* fLabel = new QLabel(QStringLiteral("⚡  Fastest server"), fastestRow);
    fLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont bold = fLabel->font(); bold.setBold(true); bold.setItalic(true);
    fLabel->setFont(bold);
    fLabel->setStyleSheet(QStringLiteral("color: #ab8fff;"));
    fbox->addWidget(fLabel, 1, Qt::AlignVCenter);

    fastestItem->setSizeHint(QSize(0, 34));
    m_list->addItem(fastestItem);
    m_list->setItemWidget(fastestItem, fastestRow);

    // ── City entries ──────────────────────────────────────────────────────
    for (const auto& [city, features] : cities)
    {
        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, city);

        auto* row = new QWidget();
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(10, 6, 10, 6);
        hbox->setSpacing(8);

        auto* cityLabel = new QLabel(city, row);
        cityLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(cityLabel, 1, Qt::AlignVCenter);

        const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto& meta : kLocationFeatures)
        {
            bool matched = false;
            for (const QString& tag : tags)
                if (tag.trimmed().contains(meta.keyword, Qt::CaseInsensitive))
                    { matched = true; break; }
            if (!matched) continue;

            auto* iconLabel = new QLabel(row);
            iconLabel->setPixmap(GeoUtils::svgPixmap(meta.resource, 16));
            iconLabel->setFixedSize(22, 22);
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setToolTip(meta.tooltip);
            hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
        }

        item->setSizeHint(QSize(0, 34));
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
    }

    m_list->setCurrentRow(0);
    m_selectedCity.clear();
}

// ============================================================
// VpnPage implementation
// ============================================================

VpnPage::VpnPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager),
      m_localCountryCode(GeoUtils::detectUserCountry())
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(0);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // ── Fixed top section: logo + power button + status label ────────────
    // This part never scrolls so the power button is always reachable.
    auto* topWidget = new QWidget(this);
    topWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* topLayout = new QVBoxLayout(topWidget);
    topLayout->setSpacing(24);
    topLayout->setContentsMargins(40, 40, 40, 8);

    // Proton VPN logo banner
    auto* logoWidget = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), 4.0, topWidget);
    topLayout->addWidget(logoWidget);

    // Power button
    m_powerBtn = new PowerButton(topWidget);
    connect(m_powerBtn, &PowerButton::clicked, this, [this]()
    {
        if (m_currentState == VpnState::Connected)
            emit disconnectRequested();
        else if (m_currentState == VpnState::Disconnected || m_currentState == VpnState::Error)
        {
            m_activeCity = m_locationPicker->selectedCity();
            emit connectRequested(m_localCountryCode, m_activeCity);
        }
    });
    topLayout->addWidget(m_powerBtn, 0, Qt::AlignCenter);

    // Status text
    m_statusLabel = new QLabel(QStringLiteral("Checking…"), topWidget);
    m_statusLabel->setObjectName(QStringLiteral("vpnStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    topLayout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    // Location picker — fixed, never scrolls
    m_locationPicker = new LocationPicker(m_localCountryCode, topWidget);
    topLayout->addWidget(m_locationPicker, 0, Qt::AlignCenter);

    outerLayout->addWidget(topWidget);

    // ── Scrollable section: timer, info, hint, button ────────────────────
    // Wrapped in a QScrollArea so it gracefully scrolls when the window is
    // at its minimum height and the content would otherwise overlap the button.
    auto* scrollContent = new QWidget();
    scrollContent->setObjectName(QStringLiteral("vpnScrollContent"));
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(16);
    scrollLayout->setContentsMargins(40, 8, 40, 16);


    // Timer label
    m_timerLabel = new QLabel(scrollContent);
    m_timerLabel->setObjectName(QStringLiteral("timerLabel"));
    m_timerLabel->setAlignment(Qt::AlignCenter);
    m_timerLabel->setVisible(false);
    scrollLayout->addWidget(m_timerLabel, 0, Qt::AlignCenter);

    // Info label (IP / error details)
    m_infoLabel = new QLabel(scrollContent);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    scrollLayout->addWidget(m_infoLabel);

    // Sign-out hint — shown only when a CLI error is detected
    m_signOutHintLabel = new QLabel(scrollContent);
    m_signOutHintLabel->setObjectName(QStringLiteral("signOutHintLabel"));
    m_signOutHintLabel->setAlignment(Qt::AlignCenter);
    m_signOutHintLabel->setWordWrap(true);
    m_signOutHintLabel->setTextFormat(Qt::RichText);
    m_signOutHintLabel->setOpenExternalLinks(false);
    m_signOutHintLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_signOutHintLabel->setVisible(false);
    connect(m_signOutHintLabel, &QLabel::linkActivated, this, [this](const QString& link)
    {
        if (link == QLatin1String("action://signout"))
            emit signOutRequested();
    });
    scrollLayout->addWidget(m_signOutHintLabel);

    // "View Details" button – shown only on error
    m_errorDetailsBtn = new QPushButton(QStringLiteral("View Details"), scrollContent);
    m_errorDetailsBtn->setVisible(false);
    m_errorDetailsBtn->setFixedWidth(140);
    connect(m_errorDetailsBtn, &QPushButton::clicked, this, &VpnPage::showErrorDetails);
    scrollLayout->addWidget(m_errorDetailsBtn, 0, Qt::AlignCenter);

    scrollLayout->addStretch(1);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("vpnScrollArea"));
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Transparent so the page background shows through
    scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea#vpnScrollArea { background: transparent; }"
        "QScrollArea#vpnScrollArea > QWidget > QWidget { background: transparent; }"));
    scrollContent->setAutoFillBackground(false);

    outerLayout->addWidget(scrollArea, 1);

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

    // Populate city combo from the detected local country (if any)
    connect(m_manager, &VpnManager::citiesReady,
            this, &VpnPage::onCitiesReady);
    if (!m_localCountryCode.isEmpty())
        m_manager->fetchCities(m_localCountryCode);

    // When the user changes location while connected/connecting, ask what to do
    connect(m_locationPicker, &LocationPicker::selectionChanged,
            this, [this](const QString& city)
    {
        if (m_currentState != VpnState::Connected && m_currentState != VpnState::Connecting)
            return;

        // No confirmation needed if the selected location matches the active connection
        if (city == m_activeCity)
            return;

        const QString locationName = city.isEmpty()
            ? QStringLiteral("Fastest server")
            : city;

        auto* dlg = new QDialog(this);
        dlg->setWindowTitle(QStringLiteral("Change Location?"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setModal(true);
        dlg->setMinimumWidth(360);

        auto* layout = new QVBoxLayout(dlg);
        layout->setSpacing(16);
        layout->setContentsMargins(24, 24, 24, 20);

        auto* msgLabel = new QLabel(
            QStringLiteral("You selected <b>%1</b>.<br>"
                           "Would you like to connect to this location now, "
                           "or use it on the next reconnect?").arg(locationName.toHtmlEscaped()),
            dlg);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        layout->addWidget(msgLabel);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(8);

        auto* laterBtn = new QPushButton(QStringLiteral("On next reconnect"), dlg);
        laterBtn->setObjectName(QStringLiteral("secondaryButton"));

        auto* nowBtn = new QPushButton(QStringLiteral("Connect now"), dlg);
        nowBtn->setObjectName(QStringLiteral("primaryButton"));
        nowBtn->setDefault(true);

        // Ensure both buttons are the same height.
        // We use the primary button's natural height as the reference — compute it
        // from its stylesheet (font metrics + 10px top + 10px bottom padding).
        const int btnH = nowBtn->sizeHint().height();
        laterBtn->setFixedHeight(btnH);
        nowBtn->setFixedHeight(btnH);

        btnRow->addWidget(laterBtn, 1);
        btnRow->addWidget(nowBtn, 1);
        layout->addLayout(btnRow);

        connect(laterBtn, &QPushButton::clicked, dlg, &QDialog::reject);
        connect(nowBtn,   &QPushButton::clicked, dlg, &QDialog::accept);

        if (dlg->exec() == QDialog::Accepted)
        {
            m_activeCity = city;
            emit connectRequested(m_localCountryCode, city);
        }
    });
}

void VpnPage::onCitiesReady(const QString& countryCode,
                            const QList<QPair<QString, QString>>& cities)
{
    // Only populate for our detected local country
    if (countryCode.compare(m_localCountryCode, Qt::CaseInsensitive) != 0)
        return;

    m_locationPicker->populate(cities);
    // populate() calls setVisible(true) internally; nothing else needed here.
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
    m_signOutHintLabel->setVisible(false);
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
        m_activeCity.clear();
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
            m_signOutHintLabel->setText(QStringLiteral(
                "<span style='color:#f5a623;'>&#9888;</span>"
                " CLI errors can sometimes be resolved by "
                "<a href='action://signout' style='color:#ab8fff;'>signing out and signing back in</a>."));
            m_signOutHintLabel->setVisible(true);
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

