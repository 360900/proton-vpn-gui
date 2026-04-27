#include "vpnpage.h"
#include "../geoutils.h"
#include "../connectionhistory.h"
#include "../uihelpers.h"
#include "../widgets/svgbanner.h"

#include <QFile>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSvgRenderer>
#include <QPropertyAnimation>
#include <QEnterEvent>
#include <QDialog>
#include <QPlainTextEdit>
#include <QFont>
#include <QGuiApplication>
#include <QClipboard>
#include <QCursor>
#include <QScrollArea>
#include <QVersionNumber>
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
        QColor arcColor(0xa0, 0xa0, 0xa0);
        ringPen.setColor(arcColor);
        p.setPen(ringPen);
        const int startAngle = static_cast<int>((90.0 - m_spinAngle) * 16.0);
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
// LocationPicker implementation
// ============================================================

LocationPicker::LocationPicker(const QString& countryCode, const QString& countryName, QWidget* parent)
    : PickerBase(parent), m_countryCode(countryCode), m_countryName(countryName)
{
    setObjectName(QStringLiteral("locationPicker"));
    setFixedWidth(260);

    // ── Header row (always visible, acts as the button) ──────────────────
    m_header = new QFrame(this);
    m_header->setObjectName(QStringLiteral("locationPickerHeader"));
    m_header->setCursor(Qt::PointingHandCursor);

    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(10, 8, 10, 8);
    headerLayout->setSpacing(10);

    // Flag
    m_flagLabel = new QLabel(m_header);
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

    m_topLine = new ElideLabel(QStringLiteral("Selected Location"), m_header);
    m_topLine->setObjectName(QStringLiteral("locationPickerTop"));

    m_bottomLine = new ElideLabel(QStringLiteral("⚡  Fastest server"), m_header);
    m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));

    textCol->addWidget(m_topLine);
    textCol->addWidget(m_bottomLine);
    headerLayout->addLayout(textCol, 1);

    // Chevron
    m_chevron = new QLabel(QStringLiteral("▾"), m_header);
    m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
    headerLayout->addWidget(m_chevron);

    // ── Outer layout ─────────────────────────────────────────────────────
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_header);

    // ── Popup ─────────────────────────────────────────────────────────────
    initPopup();
    m_header->installEventFilter(this);
    connect(m_list, &QListWidget::itemClicked, this, &LocationPicker::onRowClicked);

    // Start in loading state immediately
    setLoading(true);
}

bool LocationPicker::eventFilter(QObject* obj, QEvent* ev)
{
    // In free mode block the popup from opening on header click.
    if (m_freeMode && ev->type() == QEvent::MouseButtonRelease)
    {
        auto* w = qobject_cast<QWidget*>(obj);
        if (w && w->objectName() == QLatin1String("locationPickerHeader"))
            return true; // consume — do not open popup
    }
    if (handleCommonEvents(obj, ev))
        return true;
    return PickerBase::eventFilter(obj, ev);
}

void LocationPicker::setFreeMode(const bool free)
{
    m_freeMode = free;
    if (m_header)
    {
        m_header->setCursor(free ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
        m_header->setToolTip(free
            ? QStringLiteral("Location selection requires Proton VPN Plus.\n"
                              "Proton will pick a free server for you automatically.")
            : QString());
    }
    // Dim the top-line label to hint the control is inactive.
    if (m_topLine)
        m_topLine->setStyleSheet(free ? QStringLiteral("color: #666677;") : QString());
    if (m_bottomLine)
        m_bottomLine->setStyleSheet(free ? QStringLiteral("color: #666677;") : QString());
    if (m_chevron)
        m_chevron->setVisible(!free);
}

void LocationPicker::onRowClicked(QListWidgetItem* item)
{
    const QString clicked = item->data(Qt::UserRole).toString();
    closePopup();

    if (clicked == QLatin1String("__change_country__"))
    {
        emit changeCountryRequested();
        return;
    }

    if (!m_unknownConnection && clicked == m_selectedCity)
        return;

    m_unknownConnection = false;
    m_selectedCity = clicked;
    updateHeader();
    emit selectionChanged(m_selectedCity);
}

void LocationPicker::updateHeader() const
{
    if (!m_selectedCity.isEmpty())
    {
        m_bottomLine->setText(m_selectedCity);
    }
    else if (m_unknownConnection)
    {
        m_bottomLine->setText(QStringLiteral("Active connection"));
    }
    else if (!m_countryName.isEmpty())
    {
        m_bottomLine->setText(QStringLiteral("⚡  Fastest in %1").arg(m_countryName));
    }
    else
    {
        m_bottomLine->setText(QStringLiteral("⚡  Fastest server"));
    }
}

void LocationPicker::setLoading(bool loading)
{
    if (loading)
    {
        if (!m_loadingTimer)
        {
            m_loadingTimer = new QTimer(this);
            m_loadingTimer->setInterval(120);
            connect(m_loadingTimer, &QTimer::timeout, this, [this]()
            {
                m_loadingFrame = (m_loadingFrame + 1) % kSpinnerFrameCount;
                m_bottomLine->setText(
                    QStringLiteral("%1 Loading locations…")
                        .arg(QString::fromUtf8(kSpinnerFrames[m_loadingFrame])));
            });
        }
        m_bottomLine->setText(
            QStringLiteral("%1 Loading locations…")
                .arg(QString::fromUtf8(kSpinnerFrames[0])));
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

void LocationPicker::setUnknownConnection(bool unknown)
{
    m_unknownConnection = unknown;
    updateHeader();
}

void LocationPicker::setSelectedCity(const QString& city)
{
    m_unknownConnection = false;
    m_selectedCity = city;
    updateHeader();

    for (int i = 0; i < m_list->count(); ++i)
    {
        const QListWidgetItem* item = m_list->item(i);
        if (item->data(Qt::UserRole).toString() == city)
        {
            m_list->setCurrentRow(i);
            return;
        }
    }
    m_list->setCurrentRow(0);
}

bool LocationPicker::trySelectCity(const QString& city)
{
    for (int i = 0; i < m_list->count(); ++i)
    {
        if (m_list->item(i)->data(Qt::UserRole).toString() == city)
        {
            m_unknownConnection = false;
            m_selectedCity = city;
            updateHeader();
            m_list->setCurrentRow(i);
            return true;
        }
    }
    // City not in the list – show "Active connection" as fallback.
    m_selectedCity.clear();
    setUnknownConnection(true);
    return false;
}

void LocationPicker::populate(const QList<QPair<QString, QString>>& cities)
{
    setLoading(false);
    m_list->clear();

    // ── Fastest server entry ──────────────────────────────────────────────
    auto* fastestItem = new QListWidgetItem();
    fastestItem->setData(Qt::UserRole, QString());

    auto* fastestRow = new QWidget();
    fastestRow->setCursor(Qt::PointingHandCursor);
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

    auto* fLabel = new ElideLabel(QStringLiteral("⚡  Fastest server"), fastestRow);
    fLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont bold = fLabel->font(); bold.setBold(true); bold.setItalic(true);
    fLabel->setFont(bold);
    fLabel->setStyleSheet(QStringLiteral("color: #ab8fff;"));
    fbox->addWidget(fLabel, 1, Qt::AlignVCenter);

    fastestItem->setSizeHint(QSize(0, 34));
    m_list->addItem(fastestItem);
    m_list->setItemWidget(fastestItem, fastestRow);
    installOnRowWidget(fastestRow);

    // ── "Change country" action item ──────────────────────────────────────
    auto* changeItem = new QListWidgetItem();
    changeItem->setData(Qt::UserRole, QStringLiteral("__change_country__"));

    auto* changeRow = new QWidget();
    changeRow->setCursor(Qt::PointingHandCursor);
    auto* cbox = new QHBoxLayout(changeRow);
    cbox->setContentsMargins(10, 6, 10, 6);
    cbox->setSpacing(8);

    auto* cLabel = new ElideLabel(QStringLiteral("🌐  Change country…"), changeRow);
    cLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont italicFont = cLabel->font();
    italicFont.setItalic(true);
    cLabel->setFont(italicFont);
    cLabel->setStyleSheet(QStringLiteral("color: #888;"));
    cbox->addWidget(cLabel, 1, Qt::AlignVCenter);

    changeItem->setSizeHint(QSize(0, 34));
    m_list->addItem(changeItem);
    m_list->setItemWidget(changeItem, changeRow);
    installOnRowWidget(changeRow);

    // ── City entries ──────────────────────────────────────────────────────
    for (const auto& [city, features] : cities)
    {
        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, city);

        auto* row = new QWidget();
        row->setCursor(Qt::PointingHandCursor);
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(10, 6, 10, 6);
        hbox->setSpacing(8);

        auto* cityLabel = new ElideLabel(city, row);
        cityLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(cityLabel, 1, Qt::AlignVCenter);

        const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto& meta : kServerFeatures)
        {
            bool matched = false;
            for (const QString& tag : tags)
                if (tag.trimmed().contains(QLatin1String(meta.keyword), Qt::CaseInsensitive))
                    { matched = true; break; }
            if (!matched) continue;

            auto* iconLabel = new QLabel(row);
            iconLabel->setPixmap(GeoUtils::svgPixmap(QLatin1String(meta.resource), 16));
            iconLabel->setFixedSize(22, 22);
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setToolTip(QLatin1String(meta.tooltip));
            hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
        }

        item->setSizeHint(QSize(0, 34));
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
        installOnRowWidget(row);
    }

    m_list->setCurrentRow(0);
    m_selectedCity.clear();
}

// ============================================================
// RecentPicker implementation
// ============================================================

RecentPicker::RecentPicker(QWidget* parent)
    : PickerBase(parent)
{
    setObjectName(QStringLiteral("locationPicker")); // reuse same stylesheet
    setFixedWidth(260);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("locationPickerHeader"));
    header->setCursor(Qt::PointingHandCursor);

    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 8, 10, 8);
    hl->setSpacing(10);

    // Clock icon — mirrors the flag icon in LocationPicker for visual parity
    auto* clockIcon = new QLabel(QStringLiteral("🕐"), header);
    clockIcon->setFixedSize(28, 21);
    clockIcon->setAlignment(Qt::AlignCenter);
    hl->addWidget(clockIcon);

    auto* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    textCol->setContentsMargins(0, 0, 0, 0);

    m_topLine = new ElideLabel(QStringLiteral("Recent Connections"), header);
    m_topLine->setObjectName(QStringLiteral("locationPickerTop"));

    m_bottomLine = new ElideLabel(QStringLiteral("None yet"), header);
    m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));

    textCol->addWidget(m_topLine);
    textCol->addWidget(m_bottomLine);
    hl->addLayout(textCol, 1);

    m_chevron = new QLabel(QStringLiteral("▾"), header);
    m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
    hl->addWidget(m_chevron);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);

    initPopup();
    header->installEventFilter(this);

    connect(m_list, &QListWidget::itemClicked, this, &RecentPicker::onRowClicked);

    refresh();
}

void RecentPicker::refresh()
{
    m_list->clear();
    const auto entries = ConnectionHistory::instance().entries();

    if (entries.isEmpty())
    {
        m_bottomLine->setText(QStringLiteral("None yet"));
        m_chevron->setVisible(false);
        return;
    }

    m_chevron->setVisible(true);
    // Show most recent in header
    const auto& first = entries.first();
    const QString firstLabel = first.city.isEmpty()
        ? QStringLiteral("⚡  Fastest in %1").arg(first.countryName)
        : QStringLiteral("%1, %2").arg(first.countryName, first.city);
    m_bottomLine->setText(firstLabel);

    for (const auto& e : entries)
    {
        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole,     e.countryCode);
        item->setData(Qt::UserRole + 1, e.city);

        auto* row = new QWidget();
        row->setCursor(Qt::PointingHandCursor);
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(10, 6, 10, 6);
        hbox->setSpacing(8);

        // Flag
        auto* flagLbl = new QLabel(row);
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + e.countryCode.toLower(), 20);
        if (!pm.isNull()) { flagLbl->setPixmap(pm); flagLbl->setFixedSize(24, 18); }
        hbox->addWidget(flagLbl, 0, Qt::AlignVCenter);

        // Text
        const QString label = e.city.isEmpty()
            ? QStringLiteral("⚡  Fastest in %1").arg(e.countryName)
            : QStringLiteral("%1, %2").arg(e.countryName, e.city);
        auto* lbl = new ElideLabel(label, row);
        lbl->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(lbl, 1, Qt::AlignVCenter);

        // Date
        auto* dateLbl = new QLabel(e.connectedAt.toString(QStringLiteral("MMM d")), row);
        dateLbl->setObjectName(QStringLiteral("locationPickerTop"));
        hbox->addWidget(dateLbl, 0, Qt::AlignVCenter);

        item->setSizeHint(QSize(0, 34));
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
        installOnRowWidget(row);
    }
}

bool RecentPicker::eventFilter(QObject* obj, QEvent* ev)
{
    if (handleCommonEvents(obj, ev))
        return true;
    return PickerBase::eventFilter(obj, ev);
}

void RecentPicker::onRowClicked(QListWidgetItem* item)
{
    const QString code = item->data(Qt::UserRole).toString();
    const QString city = item->data(Qt::UserRole + 1).toString();
    closePopup();
    if (!code.isEmpty())
        emit connectionSelected(code, city);
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
    auto* topWidget = new QWidget(this);
    topWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* topLayout = new QVBoxLayout(topWidget);
    topLayout->setSpacing(24);
    topLayout->setContentsMargins(40, 40, 40, 8);

    // Proton VPN logo banner
    auto* logoWidget = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), 4.0, topWidget);
    topLayout->addWidget(logoWidget, 0, Qt::AlignCenter);

    // Power button
    m_powerBtn = new PowerButton(topWidget);
    connect(m_powerBtn, &PowerButton::clicked, this, [this]()
    {
        if (m_currentState == VpnState::Connected)
            emit disconnectRequested();
        else if (m_currentState == VpnState::Disconnected || m_currentState == VpnState::Error)
        {
            if (m_isFreeUser)
            {
                // Free users: let protonvpn pick the best free server automatically.
                emit connectRequested(QString(), QString());
            }
            else
            {
                m_activeCity = m_locationPicker->selectedCity();
                emit connectRequested(m_localCountryCode, m_activeCity);
            }
        }
    });
    topLayout->addWidget(m_powerBtn, 0, Qt::AlignCenter);

    // Status text
    m_statusLabel = new QLabel(QStringLiteral("Checking…"), topWidget);
    m_statusLabel->setObjectName(QStringLiteral("vpnStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    topLayout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    // Location picker + Recent picker
    const QString localCountryName = m_localCountryCode.isEmpty()
        ? QString()
        : GeoUtils::countryCodeToName(m_localCountryCode);
    m_locationPicker = new LocationPicker(m_localCountryCode, localCountryName, topWidget);
    connect(m_locationPicker, &LocationPicker::changeCountryRequested,
            this, &VpnPage::changeCountryRequested);

    m_recentPicker = new RecentPicker(topWidget);
    connect(m_recentPicker, &RecentPicker::connectionSelected,
            this, [this](const QString& code, const QString& city)
            {
                if (m_currentState == VpnState::Connected || m_currentState == VpnState::Connecting)
                {
                    m_locationPicker->setSelectedCity(city);
                    emit m_locationPicker->selectionChanged(city);
                }
                else
                {
                    m_activeCity = city;
                    emit connectRequested(code, city);
                }
            });

    auto* pickerContainer = new QWidget(topWidget);
    m_pickerRow = new QHBoxLayout(pickerContainer);
    m_pickerRow->setContentsMargins(0, 0, 0, 0);
    m_pickerRow->setSpacing(12);
    m_pickerRow->addWidget(m_locationPicker, 1);
    m_pickerRow->addWidget(m_recentPicker, 1);
    topLayout->addWidget(pickerContainer, 0, Qt::AlignHCenter);

    relayoutPickers(width());

    outerLayout->addWidget(topWidget);

    // ── Scrollable section: timer, info, hint, button ────────────────────
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

    // ── Port forwarding row ───────────────────────────────────────────────
    // Hidden by default; appears when natpmpc successfully allocates a port.
    m_portRow = new QWidget(scrollContent);
    auto* portRowLayout = new QHBoxLayout(m_portRow);
    portRowLayout->setContentsMargins(0, 4, 0, 4);
    portRowLayout->setSpacing(10);

    auto* portTitleLabel = new QLabel(QStringLiteral("Forwarded Port:"), m_portRow);
    portTitleLabel->setObjectName(QStringLiteral("infoLabel"));
    portRowLayout->addWidget(portTitleLabel, 0, Qt::AlignVCenter);

    m_portLabel = new QLabel(QStringLiteral("—"), m_portRow);
    m_portLabel->setObjectName(QStringLiteral("portValueLabel"));
    {
        QFont f = m_portLabel->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 1);
        m_portLabel->setFont(f);
    }
    portRowLayout->addWidget(m_portLabel, 0, Qt::AlignVCenter);

    auto* portCopyBtn = new QPushButton(QStringLiteral("Copy"), m_portRow);
    portCopyBtn->setObjectName(QStringLiteral("secondaryButton"));
    portCopyBtn->setFixedHeight(26);
    portCopyBtn->setCursor(Qt::PointingHandCursor);
    connect(portCopyBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_natPmpManager && m_natPmpManager->forwardedPort() > 0)
            QGuiApplication::clipboard()->setText(
                QString::number(m_natPmpManager->forwardedPort()));
    });
    portRowLayout->addWidget(portCopyBtn, 0, Qt::AlignVCenter);

    m_portRow->setVisible(false);
    scrollLayout->addWidget(m_portRow, 0, Qt::AlignCenter);

    scrollLayout->addStretch(1);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName(QStringLiteral("vpnScrollArea"));
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
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
    m_checkingSpinnerTimer = new QTimer(this);
    m_checkingSpinnerTimer->setInterval(200);
    connect(m_checkingSpinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_checkingSpinnerFrame = (m_checkingSpinnerFrame + 1) % kSpinnerFrameCount;
        m_statusLabel->setText(
            QStringLiteral("%1 Checking…").arg(QString::fromUtf8(kSpinnerFrames[m_checkingSpinnerFrame])));
    });

    // Start in Unknown — spinner runs until checkConnectionStatus responds
    updateUi(VpnState::Unknown, QString());
    m_checkingSpinnerTimer->start();

    // Populate city combo from the detected local country (if any)
    connect(m_manager, &VpnManager::citiesReady, this, &VpnPage::onCitiesReady);
    if (!m_localCountryCode.isEmpty())
        m_manager->fetchCities(m_localCountryCode);
    else
        m_locationPicker->populate({}); // No local country detected — stop spinner immediately

    // Check the installed CLI version against the tested version
    connect(m_manager, &VpnManager::cliVersionReady, this, &VpnPage::onCliVersionReady);
    m_manager->fetchCliVersion();

    // If the user enables port forwarding while already connected, kick off
    // the natpmpc loop immediately rather than waiting for the next reconnect.
    connect(m_manager, &VpnManager::configApplied, this, [this](const QString&)
    {
        if (m_currentState == VpnState::Connected)
            startNatPmpLoop();
    });

    // Track the country code of the currently connected server so we can look
    // up city features via fetchCityFeatures() on startup.
    connect(m_manager, &VpnManager::connectionCountryKnown, this, [this](const QString& cc)
    {
        m_connectedCountryCode = cc;
    });

    // ── NatPmpManager ────────────────────────────────────────────────────
    m_natPmpManager = new NatPmpManager(this);

    connect(m_natPmpManager, &NatPmpManager::portAcquired, this, [this](int port)
    {
        if (m_portLabel)
            m_portLabel->setText(QString::number(port));
        if (m_portRow)
            m_portRow->setVisible(true);
    });

    connect(m_natPmpManager, &NatPmpManager::portLost, this, [this]()
    {
        if (m_portRow)
            m_portRow->setVisible(false);
    });

    connect(m_natPmpManager, &NatPmpManager::natpmpcMissing, this, [this]()
    {
        if (m_natpmpcBanner)
            return; // already showing
        const auto* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
        if (!scrollArea || !scrollArea->widget()) return;
        auto* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
        if (!scrollLayout) return;

        m_natpmpcBanner = new InfoBanner(
            QStringLiteral(
                "<b>natpmpc is not installed.</b> "
                "The forwarded port cannot be displayed or kept alive automatically. "
                "Install it to use port forwarding "
                "(<code>sudo apt install natpmpc</code> on Debian/Ubuntu, "
                "<code>sudo dnf install libnatpmp</code> on Fedora, "
                "<code>sudo pacman -S libnatpmp</code> on Arch)."),
            this);
        connect(m_natpmpcBanner, &InfoBanner::dismissed, this, [this]()
        {
            m_natpmpcBanner = nullptr;
        });
        int pos = 0;
        if (m_prereleaseBanner) ++pos;
        if (m_versionBanner)    ++pos;
        scrollLayout->insertWidget(pos, m_natpmpcBanner);
    });
    // Show a banner if this is a pre-release build
    checkPrereleaseBanner();

    // React to plan type (Free vs Plus) — affects picker visibility and connect behaviour.
    connect(m_manager, &VpnManager::accountTypeReady, this, [this](AccountType type)
    {
        m_isFreeUser = (type == AccountType::Free);
        applyFreeUserMode();
    });
    // Apply immediately if already known (e.g. app restart with cached state).
    if (m_manager->accountType() != AccountType::Unknown)
    {
        m_isFreeUser = (m_manager->accountType() == AccountType::Free);
        applyFreeUserMode();
    }

    // When the user changes location while connected/connecting, ask what to do
    connect(m_locationPicker, &LocationPicker::selectionChanged,
            this, [this](const QString& city)
    {
        if (m_currentState != VpnState::Connected && m_currentState != VpnState::Connecting)
            return;

        if (!m_hadUnknownConnection && city == m_activeCity)
            return;

        m_hadUnknownConnection = false;

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

void VpnPage::notifyExternalConnect(const QString& city)
{
    m_activeCity = city;
    m_locationPicker->setSelectedCity(city);
    if (m_recentPicker) m_recentPicker->refresh();
}

void VpnPage::onStatusCityKnown(const QString& city)
{
    // Store the city so updateUi() (triggered by the subsequent
    // connectionStateChanged signal) can skip the "Active connection" fallback,
    // and applyPendingStatusCity() can select it once the list is populated.
    m_pendingStatusCity = city;
    m_activeCity        = city;
}

void VpnPage::applyPendingStatusCity()
{
    if (m_pendingStatusCity.isEmpty())
        return;

    const bool found = m_locationPicker->trySelectCity(m_pendingStatusCity);
    if (!found)
    {
        // City not in the list – treat as unknown active connection.
        m_activeCity.clear();
        m_hadUnknownConnection = true;
    }
    m_pendingStatusCity.clear();
}

void VpnPage::refreshRecentPicker()
{
    if (m_recentPicker)
        m_recentPicker->refresh();
    relayoutPickers(width());
}

void VpnPage::relayoutPickers(const int w) const
{
    if (!m_recentPicker) return;

    // Free users never see the recent connections picker.
    if (m_isFreeUser)
    {
        m_recentPicker->setVisible(false);
        m_locationPicker->setFixedWidth(260);
        return;
    }

    const bool hasHistory = !ConnectionHistory::instance().entries().isEmpty();
    if (!hasHistory)
    {
        m_recentPicker->setVisible(false);
        return;
    }

    const bool wide = w >= kWideThreshold;
    if (wide)
    {
        // Side-by-side: set equal fixed widths
        m_locationPicker->setFixedWidth(240);
        m_recentPicker->setFixedWidth(240);
        m_recentPicker->setVisible(true);
        m_pickerRow->setDirection(QBoxLayout::LeftToRight);
    }
    else
    {
        // Stacked: full width each
        m_locationPicker->setFixedWidth(260);
        m_recentPicker->setFixedWidth(260);
        m_recentPicker->setVisible(true);
        m_pickerRow->setDirection(QBoxLayout::TopToBottom);
    }
}

void VpnPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayoutPickers(event->size().width());
}

void VpnPage::onCitiesReady(const QString& countryCode,
                            const QList<QPair<QString, QString>>& cities)
{
    if (countryCode.compare(m_localCountryCode, Qt::CaseInsensitive) != 0)
        return;

    if (!m_stateKnown)
    {
        m_pendingCities = cities; // wraps in optional — even an empty list is "received"
        return;
    }

    m_locationPicker->populate(cities);
    applyPendingStatusCity();

    // Update the tracked features for the currently active city so
    // refreshConnectedInfoLabel() can show "Port forwarding is active" when
    // the app starts with the VPN already connected to a P2P server.
    if (!m_activeCity.isEmpty())
    {
        for (const auto& [city, features] : cities)
        {
            if (city.compare(m_activeCity, Qt::CaseInsensitive) == 0)
            {
                m_currentCityFeatures = features;
                break;
            }
        }
    }
    if (m_currentState == VpnState::Connected)
        refreshConnectedInfoLabel();
}

void VpnPage::checkPrereleaseBanner()
{
    QFile vf(QStringLiteral(":/version.json"));
    if (!vf.open(QIODevice::ReadOnly)) return;

    const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
    vf.close();

    if (!obj.value(QStringLiteral("prerelease")).toBool(false)) return;

    const QString appVersion = obj.value(QStringLiteral("app_version")).toString();
    const QString msg = QStringLiteral(
        "You are running a <b>pre-release</b> version of this app (<b>v%1</b>). "
        "It may contain bugs or incomplete features. Use with caution.")
        .arg(appVersion.toHtmlEscaped());

    const auto* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
    if (!scrollArea || !scrollArea->widget()) return;
    auto* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
    if (!scrollLayout) return;

    m_prereleaseBanner = new InfoBanner(msg, this);
    connect(m_prereleaseBanner, &InfoBanner::dismissed, this, [this]() {
        m_prereleaseBanner = nullptr;
    });
    scrollLayout->insertWidget(0, m_prereleaseBanner);
}

void VpnPage::onCliVersionReady(const QString& version)
{
    QString testedVersionStr;
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly))
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        testedVersionStr = obj.value(QStringLiteral("cli_version_tested")).toString();
    }

    if (version.isEmpty() || testedVersionStr.isEmpty()) return;

    const QVersionNumber installed = QVersionNumber::fromString(version);
    const QVersionNumber tested    = QVersionNumber::fromString(testedVersionStr);
    if (installed == tested) return;

    const QString msg = (installed > tested)
        ? QStringLiteral(
              "Your Proton VPN CLI (<b>v%1</b>) is newer than the version this app was "
              "tested against (<b>v%2</b>). Things may work fine, but you could encounter "
              "unexpected behavior.")
              .arg(version, testedVersionStr)
        : QStringLiteral(
              "Your Proton VPN CLI (<b>v%1</b>) is older than the version this app was "
              "tested against (<b>v%2</b>). Some features may not work correctly. "
              "Consider upgrading the CLI.")
              .arg(version, testedVersionStr);

    const auto* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
    if (!scrollArea || !scrollArea->widget()) return;
    auto* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
    if (!scrollLayout) return;

    m_versionBanner = new InfoBanner(msg, this);
    connect(m_versionBanner, &InfoBanner::dismissed, this, [this]() {
        m_versionBanner = nullptr;
    });
    const int pos = (m_prereleaseBanner != nullptr) ? 1 : 0;
    scrollLayout->insertWidget(pos, m_versionBanner);
}

void VpnPage::onStateChanged(const VpnState state, const QString& info)
{
    updateUi(state, info);
}

void VpnPage::updateUi(const VpnState state, const QString& info)
{
    const VpnState prevState = m_currentState;
    m_currentState = state;

    if (state != VpnState::Unknown)
        m_checkingSpinnerTimer->stop();

    const bool justBecameKnown = !m_stateKnown && state != VpnState::Unknown;
    if (justBecameKnown)
        m_stateKnown = true;

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
        m_lastConnectedInfo = info;
        refreshConnectedInfoLabel();
        startNatPmpLoop();

        // On app startup with VPN already connected, the CLI connect output is
        // not available, so "Port forwarding is active on this server." won't be
        // in `info`.  Fetch the city features explicitly and update the label once
        // we know whether it's a P2P server.
        if (m_manager->portForwardingEnabled() &&
            !m_connectedCountryCode.isEmpty() && !m_activeCity.isEmpty())
        {
            const QString cc   = m_connectedCountryCode;
            const QString city = m_activeCity;
            m_manager->fetchCityFeatures(cc, city, [this, cc, city](const QString& features)
            {
                // Guard: still connected to the same server when the result arrives.
                if (m_currentState != VpnState::Connected ||
                    m_connectedCountryCode != cc || m_activeCity != city)
                    return;
                m_currentCityFeatures = features;
                refreshConnectedInfoLabel();
            });
        }
        if (prevState == VpnState::Connecting)
        {
            startElapsedTimer();
            if (!m_isFreeUser && m_recentPicker)
            {
                m_recentPicker->refresh();
                relayoutPickers(width());
            }
        }
        else if (prevState == VpnState::Connected)
        {
            // Server changed while staying connected (external CLI switch).
            // Restart the elapsed timer for the new connection and update the
            // location picker to reflect the new city.
            startElapsedTimer();
            m_hadUnknownConnection = false;
            m_locationPicker->setUnknownConnection(false);
            applyPendingStatusCity();
        }
        else
        {
            stopElapsedTimer();
            // Only fall back to "Active connection" when we have no city at
            // all.  If onStatusCityKnown() was called first, m_activeCity is
            // already set and we skip this so the picker can show the real city.
            if (prevState == VpnState::Unknown && m_activeCity.isEmpty())
            {
                m_hadUnknownConnection = true;
                m_locationPicker->setUnknownConnection(true);
            }
        }
        if (justBecameKnown && m_pendingCities.has_value())
        {
            m_locationPicker->populate(*m_pendingCities);
            m_pendingCities.reset();
            applyPendingStatusCity();
        }
        break;

    case VpnState::Disconnected:
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Disconnected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("Disconnected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(info.isEmpty() ? QString() : info);
        m_activeCity.clear();
        m_hadUnknownConnection = false;
        m_locationPicker->setUnknownConnection(false);
        stopElapsedTimer();
        if (justBecameKnown && m_pendingCities.has_value())
        {
            m_locationPicker->populate(*m_pendingCities);
            m_pendingCities.reset();
        }
        break;

    case VpnState::Connecting:
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Spinning);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Connecting…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(QString());
        stopElapsedTimer();
        break;

    case VpnState::Disconnecting:
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Spinning);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Disconnecting…"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        stopElapsedTimer();
        break;

    case VpnState::Error:
    {
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Disconnected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(QStringLiteral("Error"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #d63f3f; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        stopElapsedTimer();

        m_rawError = info;

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

void VpnPage::applyFreeUserMode()
{
    // Location picker: block/unblock user interaction.
    m_locationPicker->setFreeMode(m_isFreeUser);

    // Recent connections: never shown for free users.
    if (m_recentPicker)
        m_recentPicker->setVisible(
            !m_isFreeUser && !ConnectionHistory::instance().entries().isEmpty());

    // Re-run layout so picker widths are recalculated correctly.
    relayoutPickers(width());
}

// ---------------------------------------------------------------------------
// Port forwarding — NatPmpManager integration
// ---------------------------------------------------------------------------

void VpnPage::refreshConnectedInfoLabel()
{
    QString text = m_lastConnectedInfo;

    // Append the port-forwarding notice if:
    //   1. The setting is enabled in the VPN configuration, AND
    //   2. The active city's server features include P2P, AND
    //   3. The text doesn't already contain the notice (avoids duplicates from
    //      the live CLI connect path, which already includes it).
    const bool pfEnabled = m_manager->portForwardingEnabled();
    const bool isP2P     = m_currentCityFeatures.contains(
                               QLatin1String("p2p"), Qt::CaseInsensitive);
    const QString pfNote = QStringLiteral("Port forwarding is active on this server.");
    if (pfEnabled && isP2P && !text.contains(pfNote))
    {
        if (!text.isEmpty())
            text += QLatin1Char('\n');
        text += pfNote;
    }

    m_infoLabel->setText(text);
}

void VpnPage::startNatPmpLoop()
{
    m_natPmpManager->start();
}

void VpnPage::stopNatPmpLoop()
{
    m_natPmpManager->stop();

    m_currentCityFeatures.clear();
    m_lastConnectedInfo.clear();
    m_connectedCountryCode.clear();

    if (m_portRow)
        m_portRow->setVisible(false);
}

