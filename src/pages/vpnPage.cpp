#include "vpnPage.h"
#include "../appConfig.h"
#include "../geoUtils.h"
#include "../connectionHistory.h"
#include "../favoritesManager.h"
#include "../uiHelpers.h"
#include "../widgets/svgBanner.h"
#include "../widgets/flatpakBetaBanner.h"
#include "../widgets/starButton.h"

#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
// ReSharper disable once CppUnusedIncludeDirective
#include <QJsonDocument> // Ignore unused include warning; we do use QJsonDocument
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QPropertyAnimation>
#include <QGuiApplication>
#include <QClipboard>
#include <QCursor>
#include <QScrollArea>
#include <QVersionNumber>
#include <cmath>

// ============================================================
// PowerButton implementation
// ============================================================

namespace
{
// PowerButton geometry and animation
constexpr int   BTN_SIZE               = 160;
constexpr int   RING_WIDTH             = 7;
constexpr int   ICON_SIZE              = 80;
constexpr qreal RING_OUTER_PAD         = 4.0;
constexpr qreal RING_MARGIN            = RING_WIDTH / 2.0 + RING_OUTER_PAD;
constexpr qreal HOVER_GLOW_INSET       = RING_MARGIN + RING_WIDTH / 2.0;
constexpr int   SPIN_ARC_SPAN_16TH     = -270 * 16;
constexpr int   SPIN_ANIM_DURATION_MS  = 900;
constexpr int   LIGHTNESS_MIDPOINT     = 128;
constexpr QColor SPIN_ARC_COLOR(0xa0, 0xa0, 0xa0);
constexpr QColor RING_CONNECTED_COLOR(0x1a, 0x9c, 0x5b);
constexpr QColor RING_DISCONNECTED_COLOR(0xd6, 0x3f, 0x3f);
constexpr QColor RING_UNKNOWN_COLOR(0x55, 0x55, 0x77);
constexpr QColor HOVER_GLOW(0xff, 0xff, 0xff, 18);
constexpr QRectF POWER_ICON_RECT(
    (BTN_SIZE - ICON_SIZE) / 2.0,
    (BTN_SIZE - ICON_SIZE) / 2.0,
    ICON_SIZE,
    ICON_SIZE);

// Picker header
constexpr int   PICKER_WIDTH              = 260;
constexpr int   PICKER_H_MARGIN           = 10;
constexpr int   PICKER_V_MARGIN           = 8;
constexpr int   PICKER_SPACING            = 10;
constexpr int   FLAG_W                    = 28;
constexpr int   FLAG_H                    = 21;
constexpr int   LOADING_TIMER_INTERVAL_MS = 120;
constexpr qreal LOGO_SCALE                = 4.0;
constexpr int   SMALL_ICON_PIX            = 14;
constexpr QColor STAR_FILL_COLOR(0xFF, 0xD2, 0x4A);
constexpr QColor NOTCH_ICON_COLOR(200, 200, 220);

// Picker list rows
constexpr int   ROW_HEIGHT         = 34;
constexpr int   ROW_H_MARGIN       = 10;
constexpr int   ROW_V_MARGIN       = 6;
constexpr int   ROW_SPACING        = 8;
constexpr int   SMALL_FLAG_PIX_W   = 20;
constexpr int   SMALL_FLAG_W       = 24;
constexpr int   SMALL_FLAG_H       = 18;
constexpr int   FEATURE_ICON_PIX   = 16;
constexpr int   FEATURE_ICON_SIZE  = 22;

// VpnPage layout
constexpr int   PAGE_H_MARGIN              = 40;
constexpr int   LOGO_TOP_MARGIN            = 40;
constexpr int   TOP_SECTION_SPACING        = 24;
constexpr int   TOP_SECTION_TOP_MARGIN     = 24;
constexpr int   TOP_SECTION_BTM_MARGIN     = 8;
constexpr int   SCROLL_SECTION_SPACING     = 16;
constexpr int   SCROLL_SECTION_TOP_MARGIN  = 8;
constexpr int   SCROLL_SECTION_BTM_MARGIN  = 16;
constexpr int   PORT_ROW_V_MARGIN          = 4;
constexpr int   PORT_ROW_SPACING           = 8;
constexpr int   PORT_COPY_BTN_W            = 34;
constexpr int   PORT_COPY_BTN_MIN_H        = 30;
constexpr int   SIDEBAR_L_MARGIN           = 20;
constexpr int   SIDEBAR_B_MARGIN           = 24;
constexpr int   SIDEBAR_SPACING            = 8;
constexpr int   BANNER_SCROLL_MAX_HEIGHT_WIDE = 260;
constexpr int   DRAWER_NOTCH_W             = 28;
constexpr int   DRAWER_NOTCH_H             = 64;
constexpr int   DRAWER_NOTCH_BTN_H         = 60;

// Timers and elapsed time
constexpr int   ELAPSED_TIMER_INTERVAL_MS    = 1000;
constexpr int   CHECKING_SPINNER_INTERVAL_MS = 200;
constexpr int   SECONDS_PER_HOUR             = 3600;
constexpr int   SECONDS_PER_MINUTE           = 60;

// Change-location dialog
constexpr int   CHANGE_LOCATION_DLG_MIN_W = 360;
constexpr int   DIALOG_SPACING            = 16;
constexpr int   DIALOG_H_MARGIN           = 24;
constexpr int   DIALOG_BTM_MARGIN         = 20;
constexpr int   DIALOG_BTN_SPACING        = 8;
} // namespace

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
    m_anim->setDuration(SPIN_ANIM_DURATION_MS);
    m_anim->setLoopCount(-1); // infinite
}

void PowerButton::setState(const RingState s)
{
    if (m_state == s) return;
    m_state = s;
    if (s == RingState::Spinning)
    {
        startSpin();
    }
    else
    {
        stopSpin();
    }
    update();
}

void PowerButton::startSpin() const
{
    if (m_anim->state() != QAbstractAnimation::Running)
    {
        m_anim->start();
    }
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
    const QRectF ringRect = widgetRect.adjusted(RING_MARGIN, RING_MARGIN, -RING_MARGIN, -RING_MARGIN);

    //  ring / arc
    QPen ringPen;
    ringPen.setWidth(RING_WIDTH);
    ringPen.setCapStyle(Qt::RoundCap);

    if (m_state == RingState::Spinning)
    {
        ringPen.setColor(SPIN_ARC_COLOR);
        p.setPen(ringPen);
        const int startAngle = static_cast<int>((90.0 - m_spinAngle) * 16.0);
        p.drawArc(ringRect, startAngle, SPIN_ARC_SPAN_16TH);
    }
    else
    {
        QColor ringColor;
        if (m_state == RingState::Connected)
        {
            ringColor = RING_CONNECTED_COLOR;
        }
        else if (m_state == RingState::Disconnected)
        {
            ringColor = RING_DISCONNECTED_COLOR;
        }
        else
        {
            ringColor = RING_UNKNOWN_COLOR;
        }

        ringPen.setColor(ringColor);
        p.setPen(ringPen);
        p.drawEllipse(ringRect);
    }

    //  hover glow
    if (m_hovered == true)
    {
        p.setBrush(HOVER_GLOW);
        p.setPen(Qt::NoPen);
        p.drawEllipse(widgetRect.adjusted(HOVER_GLOW_INSET, HOVER_GLOW_INSET,
                                          -HOVER_GLOW_INSET, -HOVER_GLOW_INSET));
    }

    //  power SVG
    const bool darkMode = palette().color(QPalette::Window).lightness() < LIGHTNESS_MIDPOINT;
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
    p.drawPixmap(POWER_ICON_RECT.toRect(), iconPix);
}

void PowerButton::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        emit clicked();
    }
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
    setFixedWidth(PICKER_WIDTH);

    //  Header row (always visible, acts as the button)
    m_header = new QFrame(this);
    m_header->setObjectName(QStringLiteral("locationPickerHeader"));
    m_header->setCursor(Qt::PointingHandCursor);

    QHBoxLayout* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(PICKER_H_MARGIN, PICKER_V_MARGIN, PICKER_H_MARGIN, PICKER_V_MARGIN);
    headerLayout->setSpacing(PICKER_SPACING);

    // Flag
    m_flagLabel = new QLabel(m_header);
    m_flagLabel->setFixedSize(FLAG_W, FLAG_H);
    m_flagLabel->setAlignment(Qt::AlignCenter);
    if (countryCode.isEmpty() == false)
    {
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + countryCode.toLower(), FLAG_W);
        if (pm.isNull() == false)
        {
            m_flagLabel->setPixmap(pm);
        }
    }
    headerLayout->addWidget(m_flagLabel);

    // Two-line text block
    QVBoxLayout* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    textCol->setContentsMargins(0, 0, 0, 0);

    m_topLine = new ElideLabel(tr("Selected Location"), m_header);
    m_topLine->setObjectName(QStringLiteral("locationPickerTop"));

    m_bottomLine = new ElideLabel(tr("\u26a1  Fastest server"), m_header);
    m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));

    textCol->addWidget(m_topLine);
    textCol->addWidget(m_bottomLine);
    headerLayout->addLayout(textCol, 1);

    // Chevron
    m_chevron = new QLabel(QStringLiteral("▾"), m_header);
    m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
    headerLayout->addWidget(m_chevron);

    //  Outer layout
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_header);

    //  Popup
    initPopup();
    m_header->installEventFilter(this);
    connect(m_list, &QListWidget::itemClicked, this, &LocationPicker::onRowClicked);

    // Start in loading state immediately
    setLoading(true);
}

bool LocationPicker::eventFilter(QObject* obj, QEvent* ev)
{
    // In free mode block the popup from opening on header click.
    if (m_freeMode == true && ev->type() == QEvent::MouseButtonRelease)
    {
        const QWidget* w = qobject_cast<QWidget*>(obj);
        if (w != nullptr && w->objectName() == QLatin1String("locationPickerHeader"))
            return true; // consume - do not open popup
    }
    if (handleCommonEvents(obj, ev) == true)
        return true;
    return PickerBase::eventFilter(obj, ev);
}

void LocationPicker::setFreeMode(const bool free)
{
    m_freeMode = free;
    if (m_header != nullptr)
    {
        m_header->setCursor(free ? Qt::ForbiddenCursor : Qt::PointingHandCursor);
        m_header->setToolTip(free
            ? tr("Location selection requires Proton VPN Plus.\n"
                 "Proton will pick a free server for you automatically.")
            : QString());
    }
    // Dim the top-line label to hint the control is inactive.
    const QString freeTextStyle = free == true
        ? QStringLiteral("color: #666677;")
        : QString();

    if (m_topLine != nullptr)
    {
        m_topLine->setStyleSheet(freeTextStyle);
    }
    if (m_bottomLine != nullptr)
    {
        m_bottomLine->setStyleSheet(freeTextStyle);
    }
    if (m_chevron != nullptr)
    {
        m_chevron->setVisible(free == false && m_collapsed == false);
    }
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

    if (m_unknownConnection == false && clicked == m_selectedCity)
        return;

    m_unknownConnection = false;
    m_selectedCity = clicked;
    updateHeader();
    emit selectionChanged(m_selectedCity);
}

void LocationPicker::updateHeader() const
{
    if (m_selectedCity.isEmpty() == false)
    {
        m_bottomLine->setText(m_selectedCity);
    }
    else if (m_unknownConnection == true)
    {
        m_bottomLine->setText(tr("Active connection"));
    }
    else if (m_countryName.isEmpty() == false)
    {
        m_bottomLine->setText(tr("\u26a1  Fastest in %1").arg(m_countryName));
    }
    else
    {
        m_bottomLine->setText(tr("\u26a1  Fastest server"));
    }
}

void LocationPicker::setLoading(const bool loading)
{
    if (loading == true)
    {
        if (m_loadingTimer == nullptr)
        {
            m_loadingTimer = new QTimer(this);
            m_loadingTimer->setInterval(LOADING_TIMER_INTERVAL_MS);
            connect(m_loadingTimer, &QTimer::timeout, this, [this]()
            {
                m_loadingFrame = (m_loadingFrame + 1) % kSpinnerFrameCount;
                m_bottomLine->setText(
                    tr("%1 Loading locations\u2026")
                        .arg(QString::fromUtf8(kSpinnerFrames[m_loadingFrame])));
            });
        }
        m_bottomLine->setText(
            tr("%1 Loading locations\u2026")
                .arg(QString::fromUtf8(kSpinnerFrames[0])));
        m_loadingTimer->start();
        m_chevron->setVisible(false);
        setVisible(true);
    }
    else
    {
        if (m_loadingTimer != nullptr)
        {
            m_loadingTimer->stop();
        }

        m_chevron->setVisible(m_collapsed == false);
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
    // City not in the local country's list (e.g. connected to a different country).
    // Leave the picker display unchanged so it keeps showing the last local selection.
    return false;
}

void LocationPicker::populate(const QList<QPair<QString, QString>>& cities)
{
    setLoading(false);
    m_list->clear();

    //  Fastest server entry
    QListWidgetItem* fastestItem = new QListWidgetItem();
    fastestItem->setData(Qt::UserRole, QString());

    QWidget* fastestRow = new QWidget();
    fastestRow->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* fbox = new QHBoxLayout(fastestRow);
    fbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
    fbox->setSpacing(ROW_SPACING);

    QLabel* fIcon = new QLabel(fastestRow);
    if (m_countryCode.isEmpty() == false)
    {
        const QPixmap pm = GeoUtils::svgPixmap(QStringLiteral(":/flags/") + m_countryCode.toLower(), SMALL_FLAG_PIX_W);
        if (pm.isNull() == false)
        {
            fIcon->setPixmap(pm);
            fIcon->setFixedSize(SMALL_FLAG_W, SMALL_FLAG_H);
        }
    }
    fbox->addWidget(fIcon, 0, Qt::AlignVCenter);

    ElideLabel* fLabel = new ElideLabel(tr("\u26a1  Fastest server"), fastestRow);
    fLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont bold = fLabel->font(); bold.setBold(true); bold.setItalic(true);
    fLabel->setFont(bold);
    fLabel->setStyleSheet(QStringLiteral("color: #ab8fff;"));
    fbox->addWidget(fLabel, 1, Qt::AlignVCenter);

    if (AppConfig::instance().favoritesEnabled() == true && m_countryCode.isEmpty() == false)
    {
        const QString countryName = GeoUtils::countryCodeToName(m_countryCode);
        fbox->addWidget(makeStarButton(m_countryCode, countryName, QString(), fastestRow),
                        0, Qt::AlignVCenter);
    }

    fastestItem->setSizeHint(QSize(0, ROW_HEIGHT));
    m_list->addItem(fastestItem);
    m_list->setItemWidget(fastestItem, fastestRow);
    installOnRowWidget(fastestRow);

    //  "Change country" action item
    QListWidgetItem* changeItem = new QListWidgetItem();
    changeItem->setData(Qt::UserRole, QStringLiteral("__change_country__"));

    QWidget* changeRow = new QWidget();
    changeRow->setCursor(Qt::PointingHandCursor);
    QHBoxLayout* cbox = new QHBoxLayout(changeRow);
    cbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
    cbox->setSpacing(ROW_SPACING);

    ElideLabel* cLabel = new ElideLabel(tr("\U0001f310  Change country\u2026"), changeRow);
    cLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
    QFont italicFont = cLabel->font();
    italicFont.setItalic(true);
    cLabel->setFont(italicFont);
    cLabel->setStyleSheet(QStringLiteral("color: #888;"));
    cbox->addWidget(cLabel, 1, Qt::AlignVCenter);

    changeItem->setSizeHint(QSize(0, ROW_HEIGHT));
    m_list->addItem(changeItem);
    m_list->setItemWidget(changeItem, changeRow);
    installOnRowWidget(changeRow);

    //  City entries
    for (const auto& [city, features] : cities)
    {
        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole, city);

        QWidget* row = new QWidget();
        row->setCursor(Qt::PointingHandCursor);
        QHBoxLayout* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
        hbox->setSpacing(ROW_SPACING);

        ElideLabel* cityLabel = new ElideLabel(city, row);
        cityLabel->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(cityLabel, 1, Qt::AlignVCenter);

        const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const auto& meta : kServerFeatures)
        {
            const bool matched = std::ranges::any_of(tags, [&meta](const QString& tag)
            {
                return tag.trimmed().contains(QLatin1String(meta.keyword), Qt::CaseInsensitive);
            });
            if (matched == false) continue;

            QLabel* iconLabel = new QLabel(row);
            iconLabel->setPixmap(GeoUtils::svgPixmap(QLatin1String(meta.resource), FEATURE_ICON_PIX));
            iconLabel->setFixedSize(FEATURE_ICON_SIZE, FEATURE_ICON_SIZE);
            iconLabel->setAlignment(Qt::AlignCenter);
            iconLabel->setToolTip(translatedFeatureTooltip(meta));
            hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
        }

        if (AppConfig::instance().favoritesEnabled() == true && m_countryCode.isEmpty() == false)
        {
            const QString countryName = GeoUtils::countryCodeToName(m_countryCode);
            hbox->addWidget(makeStarButton(m_countryCode, countryName, city, row),
                            0, Qt::AlignVCenter);
        }

        item->setSizeHint(QSize(0, ROW_HEIGHT));
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
    setFixedWidth(PICKER_WIDTH);

    QFrame* header = new QFrame(this);
    header->setObjectName(QStringLiteral("locationPickerHeader"));
    header->setCursor(Qt::PointingHandCursor);
    m_header = header; // store in PickerBase for setCollapsed()

    QHBoxLayout* hl = new QHBoxLayout(header);
    hl->setContentsMargins(PICKER_H_MARGIN, PICKER_V_MARGIN, PICKER_H_MARGIN, PICKER_V_MARGIN);
    hl->setSpacing(PICKER_SPACING);

    // Clock icon - mirrors the flag icon in LocationPicker for visual parity
    QLabel* clockIcon = new QLabel(QStringLiteral("🕐"), header);
    clockIcon->setFixedSize(FLAG_W, FLAG_H);
    clockIcon->setAlignment(Qt::AlignCenter);
    hl->addWidget(clockIcon);

    QVBoxLayout* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    textCol->setContentsMargins(0, 0, 0, 0);

    m_topLine = new ElideLabel(tr("Recent Connections"), header);
    m_topLine->setObjectName(QStringLiteral("locationPickerTop"));

    m_bottomLine = new ElideLabel(tr("None yet"), header);
    m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));

    textCol->addWidget(m_topLine);
    textCol->addWidget(m_bottomLine);
    hl->addLayout(textCol, 1);

    m_chevron = new QLabel(QStringLiteral("▾"), header);
    m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
    hl->addWidget(m_chevron);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
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
    const QList<ConnectionEntry> entries = ConnectionHistory::instance().entries();

    if (entries.isEmpty())
    {
        m_bottomLine->setText(tr("None yet"));
        m_chevron->setVisible(false);
        return;
    }

    m_chevron->setVisible(m_collapsed == false);
    // Show most recent in header
    const ConnectionEntry& first = entries.first();
    const QString firstLabel = first.city.isEmpty()
        ? tr("\u26a1  Fastest in %1").arg(first.countryName)
        : QStringLiteral("%1, %2").arg(first.countryName, first.city);
    m_bottomLine->setText(firstLabel);

    for (const auto& e : entries)
    {
        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole,     e.countryCode);
        item->setData(Qt::UserRole + 1, e.city);

        QWidget* row = new QWidget();
        row->setCursor(Qt::PointingHandCursor);
        QHBoxLayout* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
        hbox->setSpacing(ROW_SPACING);

        // Flag
        QLabel* flagLbl = new QLabel(row);
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + e.countryCode.toLower(), SMALL_FLAG_PIX_W);
        if (pm.isNull() == false)
        {
            flagLbl->setPixmap(pm);
            flagLbl->setFixedSize(SMALL_FLAG_W, SMALL_FLAG_H);
        }
        hbox->addWidget(flagLbl, 0, Qt::AlignVCenter);

        // Text
        const QString label = e.city.isEmpty() == true
            ? tr("\u26a1  Fastest in %1").arg(e.countryName)
            : QStringLiteral("%1, %2").arg(e.countryName, e.city);
        ElideLabel* lbl = new ElideLabel(label, row);
        lbl->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(lbl, 1, Qt::AlignVCenter);

        // Date
        QLabel* dateLbl = new QLabel(QLocale().toString(e.connectedAt, tr("MMM d")), row);
        dateLbl->setObjectName(QStringLiteral("locationPickerTop"));
        hbox->addWidget(dateLbl, 0, Qt::AlignVCenter);

        // Star button
        if (AppConfig::instance().favoritesEnabled() == true)
        {
            hbox->addWidget(makeStarButton(e.countryCode, e.countryName, e.city, row),
                            0, Qt::AlignVCenter);
        }

        item->setSizeHint(QSize(0, ROW_HEIGHT));
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
        installOnRowWidget(row);
    }
}

bool RecentPicker::eventFilter(QObject* obj, QEvent* ev)
{
    if (handleCommonEvents(obj, ev) == true)
        return true;
    return PickerBase::eventFilter(obj, ev);
}

void RecentPicker::onRowClicked(QListWidgetItem* item)
{
    const QString code = item->data(Qt::UserRole).toString();
    const QString city = item->data(Qt::UserRole + 1).toString();
    closePopup();
    if (code.isEmpty() == false)
    {
        emit connectionSelected(code, city);
    }
}

// ============================================================
// FavoritesPicker implementation
// ============================================================

FavoritesPicker::FavoritesPicker(QWidget* parent)
    : PickerBase(parent)
{
    setObjectName(QStringLiteral("locationPicker")); // reuse same stylesheet
    setFixedWidth(PICKER_WIDTH);

    QFrame* header = new QFrame(this);
    header->setObjectName(QStringLiteral("locationPickerHeader"));
    header->setCursor(Qt::PointingHandCursor);
    m_header = header; // store in PickerBase for setCollapsed()

    QHBoxLayout* hl = new QHBoxLayout(header);
    hl->setContentsMargins(PICKER_H_MARGIN, PICKER_V_MARGIN, PICKER_H_MARGIN, PICKER_V_MARGIN);
    hl->setSpacing(PICKER_SPACING);

    // Star icon
    QLabel* starIcon = new QLabel(header);
    {
        const QPixmap px = GeoUtils::svgPixmap(
            QStringLiteral(":/assets/star-fill.svg"), SMALL_ICON_PIX, STAR_FILL_COLOR);
        starIcon->setPixmap(px);
        starIcon->setFixedSize(FLAG_W, FLAG_H);
        starIcon->setAlignment(Qt::AlignCenter);
    }
    hl->addWidget(starIcon);

    QVBoxLayout* textCol = new QVBoxLayout();
    textCol->setSpacing(1);
    textCol->setContentsMargins(0, 0, 0, 0);

    m_topLine = new ElideLabel(tr("Favorites"), header);
    m_topLine->setObjectName(QStringLiteral("locationPickerTop"));

    m_bottomLine = new ElideLabel(tr("None yet"), header);
    m_bottomLine->setObjectName(QStringLiteral("locationPickerBottom"));

    textCol->addWidget(m_topLine);
    textCol->addWidget(m_bottomLine);
    hl->addLayout(textCol, 1);

    m_chevron = new QLabel(QStringLiteral("▾"), header);
    m_chevron->setObjectName(QStringLiteral("locationPickerChevron"));
    hl->addWidget(m_chevron);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(header);

    initPopup();
    header->installEventFilter(this);

    connect(m_list, &QListWidget::itemClicked, this, &FavoritesPicker::onRowClicked);

    refresh();
}

void FavoritesPicker::refresh()
{
    m_list->clear();
    const QList<FavoriteEntry> entries = FavoritesManager::instance().entries();

    if (entries.isEmpty())
    {
        m_bottomLine->setText(tr("None yet"));
        m_chevron->setVisible(false);
        return;
    }

    m_chevron->setVisible(m_collapsed == false);
    // Show first favorite in header
    const FavoriteEntry& first = entries.first();
    const QString firstLabel = first.city.isEmpty()
        ? tr("\u26a1  Fastest in %1").arg(first.countryName)
        : QStringLiteral("%1, %2").arg(first.countryName, first.city);
    m_bottomLine->setText(firstLabel);

    for (const auto& e : entries)
    {
        QListWidgetItem* item = new QListWidgetItem();
        item->setData(Qt::UserRole,     e.countryCode);
        item->setData(Qt::UserRole + 1, e.city);

        QWidget* row = new QWidget();
        row->setCursor(Qt::PointingHandCursor);
        QHBoxLayout* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(ROW_H_MARGIN, ROW_V_MARGIN, ROW_H_MARGIN, ROW_V_MARGIN);
        hbox->setSpacing(ROW_SPACING);

        // Flag
        QLabel* flagLbl = new QLabel(row);
        const QPixmap pm = GeoUtils::svgPixmap(
            QStringLiteral(":/flags/") + e.countryCode.toLower(), SMALL_FLAG_PIX_W);
        if (pm.isNull() == false)
        {
            flagLbl->setPixmap(pm);
            flagLbl->setFixedSize(SMALL_FLAG_W, SMALL_FLAG_H);
        }
        hbox->addWidget(flagLbl, 0, Qt::AlignVCenter);

        // Text
        const QString label = e.city.isEmpty() == true
            ? tr("\u26a1  Fastest in %1").arg(e.countryName)
            : QStringLiteral("%1, %2").arg(e.countryName, e.city);
        ElideLabel* lbl = new ElideLabel(label, row);
        lbl->setObjectName(QStringLiteral("locationPickerItemLabel"));
        hbox->addWidget(lbl, 1, Qt::AlignVCenter);

        // Star button (unfavorite)
        hbox->addWidget(makeStarButton(e.countryCode, e.countryName, e.city, row),
                        0, Qt::AlignVCenter);

        item->setSizeHint(QSize(0, ROW_HEIGHT));
        m_list->addItem(item);
        m_list->setItemWidget(item, row);
        installOnRowWidget(row);
    }

    // If the popup is currently open, resize it to match the new item count.
    if (m_popup != nullptr && m_popup->isVisible())
    {
        resizeList();
    }
}

bool FavoritesPicker::eventFilter(QObject* obj, QEvent* ev)
{
    if (handleCommonEvents(obj, ev) == true)
        return true;
    return PickerBase::eventFilter(obj, ev);
}

void FavoritesPicker::onRowClicked(QListWidgetItem* item)
{
    const QString code = item->data(Qt::UserRole).toString();
    const QString city = item->data(Qt::UserRole + 1).toString();
    closePopup();
    if (code.isEmpty() == false)
    {
        emit connectionSelected(code, city);
    }
}

// ============================================================

// ============================================================
// VpnPage implementation
// ============================================================

VpnPage::VpnPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager),
      m_localCountryCode(GeoUtils::detectUserCountry())
{
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    m_outerLayout = outerLayout;
    outerLayout->setSpacing(0);
    // No left margin here - logo and power button span the full page width so they
    // are visually centred.  The COLLAPSED_DRAWER_WIDTH offset is applied only to the scroll
    // area via m_scrollOffsetWidget, keeping content clear of the drawer overlay.
    outerLayout->setContentsMargins(0, 0, 0, 0);

    //  Logo row - always at the top, full width
    m_logoRow = new QWidget(this);
    m_logoRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* logoRowLayout = new QHBoxLayout(m_logoRow);
    logoRowLayout->setContentsMargins(PAGE_H_MARGIN, LOGO_TOP_MARGIN, PAGE_H_MARGIN, 0);

    // Proton VPN logo banner
    SvgBanner* logoWidget = new SvgBanner(QStringLiteral(":/assets/proton-vpn-logo.svg"), LOGO_SCALE, m_logoRow);
    logoWidget->setLightResource(QStringLiteral(":/assets/proton-vpn-logo-light.svg"));
    logoRowLayout->addWidget(logoWidget, 0, Qt::AlignCenter);

    outerLayout->addWidget(m_logoRow);

    //  Fixed top section: power button + status label
    m_topContentWidget = new QWidget(this);
    m_topContentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout* topLayout = new QVBoxLayout(m_topContentWidget);
    topLayout->setSpacing(TOP_SECTION_SPACING);
    topLayout->setContentsMargins(PAGE_H_MARGIN, TOP_SECTION_TOP_MARGIN, PAGE_H_MARGIN, TOP_SECTION_BTM_MARGIN);

    // Power button
    m_powerBtn = new PowerButton(m_topContentWidget);
    connect(m_powerBtn, &PowerButton::clicked, this, [this]()
    {
        if (m_currentState == VpnState::Connected)
        {
            emit disconnectRequested();
        }
        else if (m_currentState == VpnState::Disconnected || m_currentState == VpnState::Error)
        {
            if (m_isFreeUser == true)
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
    m_statusLabel = new QLabel(tr("Checking\u2026"), m_topContentWidget);
    m_statusLabel->setObjectName(QStringLiteral("vpnStatusLabel"));
    m_statusLabel->setAlignment(Qt::AlignCenter);
    topLayout->addWidget(m_statusLabel, 0, Qt::AlignCenter);

    // Location picker + Recent picker
    const QString localCountryName = m_localCountryCode.isEmpty()
        ? QString()
        : GeoUtils::countryCodeToName(m_localCountryCode);
    m_locationPicker = new LocationPicker(m_localCountryCode, localCountryName, this);
    connect(m_locationPicker, &LocationPicker::changeCountryRequested,
            this, &VpnPage::changeCountryRequested);

    m_recentPicker = new RecentPicker(this);
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

    // Favorites picker
    m_favoritesPicker = new FavoritesPicker(this);
    connect(m_favoritesPicker, &FavoritesPicker::connectionSelected,
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

    // Auto-refresh favorites picker when favorites change
    connect(&FavoritesManager::instance(), &FavoritesManager::changed,
            this, [this]()
            {
                if (m_favoritesPicker != nullptr)
                {
                    m_favoritesPicker->refresh();
                }
                relayoutPickers();
            });

    // Note: pickers are NOT added to topLayout here.
    // They live inside m_drawer (an absolutely-positioned overlay) created below.

    // Apply persisted visibility preference for the location picker.
    setLocationPickerVisible(AppConfig::instance().showLocationPicker());


    //  Scrollable section: timer, info, hint, button
    QWidget* scrollContent = new QWidget();
    scrollContent->setObjectName(QStringLiteral("vpnScrollContent"));
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QVBoxLayout* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(SCROLL_SECTION_SPACING);
    scrollLayout->setContentsMargins(PAGE_H_MARGIN, SCROLL_SECTION_TOP_MARGIN, PAGE_H_MARGIN, SCROLL_SECTION_BTM_MARGIN);

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

    // Sign-out hint - shown only when a CLI error is detected
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
    m_errorDetailsBtn = new QPushButton(tr("View Details"), scrollContent);
    m_errorDetailsBtn->setVisible(false);
    m_errorDetailsBtn->setFixedWidth(140);
    connect(m_errorDetailsBtn, &QPushButton::clicked, this, &VpnPage::showErrorDetails);
    scrollLayout->addWidget(m_errorDetailsBtn, 0, Qt::AlignCenter);

    //  Port forwarding row
    // Hidden by default; appears when natpmpc successfully allocates a port.
    m_portRow = new QWidget(scrollContent);
    QHBoxLayout* portRowLayout = new QHBoxLayout(m_portRow);
    portRowLayout->setContentsMargins(0, PORT_ROW_V_MARGIN, 0, PORT_ROW_V_MARGIN);
    portRowLayout->setSpacing(PORT_ROW_SPACING);

    QLabel* portTitleLabel = new QLabel(tr("Forwarded Port:"), m_portRow);
    portTitleLabel->setObjectName(QStringLiteral("infoLabel"));
    portRowLayout->addWidget(portTitleLabel, 0, Qt::AlignVCenter);

    //  Button-group container
    // Left segment : port number label
    // Right segment: clipboard icon button
    // Styled to look like a Bootstrap input-group / btn-group.
    QWidget* btnGroup = new QWidget(m_portRow);
    btnGroup->setObjectName(QStringLiteral("portBtnGroup"));
    QHBoxLayout* btnGroupLayout = new QHBoxLayout(btnGroup);
    btnGroupLayout->setContentsMargins(0, 0, 0, 0);
    btnGroupLayout->setSpacing(0);

    // Left segment - port number
    m_portLabel = new QLabel(QStringLiteral("-"), btnGroup);
    m_portLabel->setObjectName(QStringLiteral("portValueLabel"));
    m_portLabel->setAlignment(Qt::AlignCenter);
    {
        QFont f = m_portLabel->font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 1);
        m_portLabel->setFont(f);
    }
    btnGroupLayout->addWidget(m_portLabel);

    // Right segment - clipboard icon button
    // Build a white-tinted icon from the SVG asset.
    QPixmap clipPix(FEATURE_ICON_PIX, FEATURE_ICON_PIX);
    clipPix.fill(Qt::transparent);
    {
        QPainter clipPainter(&clipPix);
        QSvgRenderer clipRenderer(QStringLiteral(":/assets/clipboard2-plus.svg"));
        clipRenderer.render(&clipPainter);
        clipPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        clipPainter.fillRect(clipPix.rect(), Qt::white);
    }

    QPushButton* portCopyBtn = new QPushButton(btnGroup);
    portCopyBtn->setObjectName(QStringLiteral("portCopyBtn"));
    portCopyBtn->setIcon(QIcon(clipPix));
    portCopyBtn->setIconSize({FEATURE_ICON_PIX, FEATURE_ICON_PIX});
    portCopyBtn->setFixedSize(PORT_COPY_BTN_W, m_portLabel->sizeHint().height() > 0
                                               ? m_portLabel->sizeHint().height()
                                               : PORT_COPY_BTN_MIN_H);
    portCopyBtn->setCursor(Qt::PointingHandCursor);
    portCopyBtn->setToolTip(tr("Copy to Clipboard"));
    connect(portCopyBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_natPmpManager != nullptr && m_natPmpManager->forwardedPort() > 0)
        {
            QGuiApplication::clipboard()->setText(
                QString::number(m_natPmpManager->forwardedPort()));
        }
    });
    btnGroupLayout->addWidget(portCopyBtn);

    portRowLayout->addWidget(btnGroup, 0, Qt::AlignVCenter);

    m_portRow->setVisible(false);
    scrollLayout->addWidget(m_portRow, 0, Qt::AlignCenter);

    scrollLayout->addStretch(1);

    // Banner area: starts at the bottom of the scroll content (narrow mode).
    // applyWideMode() moves it above the picker dropdowns in the sidebar.
    m_vpnBannerArea = new QWidget();
    QVBoxLayout* bannerAreaLayout = new QVBoxLayout(m_vpnBannerArea);
    bannerAreaLayout->setContentsMargins(0, SCROLL_SECTION_SPACING, 0, 0);
    bannerAreaLayout->setSpacing(SIDEBAR_SPACING);

    // Header: "Warnings" label + "Clear All" button
    QWidget* bannerHeader = new QWidget(m_vpnBannerArea);
    QHBoxLayout* bannerHeaderLayout = new QHBoxLayout(bannerHeader);
    bannerHeaderLayout->setContentsMargins(0, 0, 0, 0);
    bannerHeaderLayout->setSpacing(SIDEBAR_SPACING);
    m_warningsHeaderLabel = new QLabel(tr("Warnings"), bannerHeader);
    m_warningsHeaderLabel->setObjectName(QStringLiteral("appSectionHeader"));
    m_clearAllBannersBtn = new QPushButton(tr("Clear All"), bannerHeader);
    m_clearAllBannersBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_clearAllBannersBtn->setCursor(Qt::PointingHandCursor);
    connect(m_clearAllBannersBtn, &QPushButton::clicked, this, [this]()
    {
        if (m_prereleaseBanner != nullptr)
        {
            m_prereleaseBanner->dismiss();
        }
        if (m_flatpakBetaBanner != nullptr)
        {
            m_flatpakBetaBanner->dismiss();
        }
        if (m_appImageBetaBanner != nullptr)
        {
            m_appImageBetaBanner->dismiss();
        }
    });
    bannerHeaderLayout->addWidget(m_warningsHeaderLabel);
    bannerHeaderLayout->addStretch();
    bannerHeaderLayout->addWidget(m_clearAllBannersBtn);
    bannerAreaLayout->addWidget(bannerHeader);

    // Banner content — added directly in narrow mode (outer scroll area handles
    // overflow).  applyWideMode() wraps it in m_vpnBannerScroll for wide mode.
    m_vpnBannerContent = new QWidget();
    m_vpnBannerLayout = new QVBoxLayout(m_vpnBannerContent);
    m_vpnBannerLayout->setContentsMargins(0, 0, 0, 0);
    m_vpnBannerLayout->setSpacing(0);
    bannerAreaLayout->addWidget(m_vpnBannerContent);

    m_vpnBannerArea->setVisible(false);
    // Insert before the final stretch so banners sit below the status content.
    scrollLayout->insertWidget(scrollLayout->count() - 1, m_vpnBannerArea);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("vpnScrollArea"));
    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea#vpnScrollArea { background: transparent; }"
        "QScrollArea#vpnScrollArea > QWidget > QWidget { background: transparent; }"));
    scrollContent->setAutoFillBackground(false);

    //  Narrow mode wrapper (default)
    m_narrowContent = new QWidget(this);
    m_narrowContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_narrowContentLayout = new QVBoxLayout(m_narrowContent);
    m_narrowContentLayout->setContentsMargins(0, 0, 0, 0);
    m_narrowContentLayout->setSpacing(0);
    m_narrowContentLayout->addWidget(m_topContentWidget);

    // Scroll offset wrapper: gives the scroll area a COLLAPSED_DRAWER_WIDTH left margin so
    // it sits to the right of the drawer overlay.  The logo row and power-button
    // section are NOT wrapped here, so they remain visually centred on the page.
    m_scrollOffsetWidget = new QWidget(m_narrowContent);
    m_scrollOffsetWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scrollOffsetLayout = new QVBoxLayout(m_scrollOffsetWidget);
    m_scrollOffsetLayout->setContentsMargins(PickerDrawer::COLLAPSED_DRAWER_WIDTH, 0, PickerDrawer::COLLAPSED_DRAWER_WIDTH, 0);
    m_scrollOffsetLayout->setSpacing(0);
    m_scrollOffsetLayout->addWidget(m_scrollArea, 1);

    m_narrowContentLayout->addWidget(m_scrollOffsetWidget, 1);
    outerLayout->addWidget(m_narrowContent, 1);

    //  Wide mode wrapper (two-column layout, initially hidden)
    m_wideContent = new QWidget(this);
    m_wideContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_wideContent->setVisible(false);

    QHBoxLayout* wideLayout = new QHBoxLayout(m_wideContent);
    wideLayout->setContentsMargins(0, 0, 0, 0);
    wideLayout->setSpacing(0);

    // Left column: picker sidebar (fixed width, pickers at bottom)
    m_pickerSidebar = new QWidget(m_wideContent);
    m_pickerSidebar->setFixedWidth(kWideSidebarW);
    m_pickerSidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_pickerSidebarLayout = new QVBoxLayout(m_pickerSidebar);
    m_pickerSidebarLayout->setContentsMargins(SIDEBAR_L_MARGIN, 0, 0, SIDEBAR_B_MARGIN);
    m_pickerSidebarLayout->setSpacing(SIDEBAR_SPACING);
    m_pickerSidebarLayout->addStretch(1); // pushes pickers to bottom

    // Right column: power button, status, scrollable content
    m_rightContent = new QWidget(m_wideContent);
    m_rightContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rightContentLayout = new QVBoxLayout(m_rightContent);
    m_rightContentLayout->setContentsMargins(0, 0, 0, 0);
    m_rightContentLayout->setSpacing(0);
    // topContentWidget and scrollArea are added here in applyWideMode(true)

    wideLayout->addWidget(m_pickerSidebar);
    wideLayout->addWidget(m_rightContent, 1);
    outerLayout->addWidget(m_wideContent, 1);

    // Elapsed timer
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(ELAPSED_TIMER_INTERVAL_MS);
    connect(m_elapsedTimer, &QTimer::timeout, this, [this]()
    {
        m_elapsedSeconds++;
        int h = m_elapsedSeconds / SECONDS_PER_HOUR;
        int m = (m_elapsedSeconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
        int s = m_elapsedSeconds % SECONDS_PER_MINUTE;
        m_timerLabel->setText(QString::asprintf("%02d:%02d:%02d", h, m, s));
    });

    // Checking spinner - animates the status label while connection state is unknown
    m_checkingSpinnerTimer = new QTimer(this);
    m_checkingSpinnerTimer->setInterval(CHECKING_SPINNER_INTERVAL_MS);
    connect(m_checkingSpinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_checkingSpinnerFrame = (m_checkingSpinnerFrame + 1) % kSpinnerFrameCount;
        m_statusLabel->setText(
            tr("%1 Checking\u2026").arg(QString::fromUtf8(kSpinnerFrames[m_checkingSpinnerFrame])));
    });

    // Start in Unknown - spinner runs until the status monitor's first snapshot arrives
    updateUi(VpnState::Unknown, QString());
    m_checkingSpinnerTimer->start();

    // Populate city combo from the detected local country (if any)
    connect(m_manager, &VpnManager::citiesReady, this, &VpnPage::onCitiesReady);
    if (m_localCountryCode.isEmpty() == false)
    {
        m_manager->fetchCities(m_localCountryCode);
    }
    else
    {
        m_locationPicker->populate({}); // No local country detected - stop spinner immediately
    }

    // Check the installed CLI version against the tested version
    connect(m_manager, &VpnManager::cliVersionReady, this, &VpnPage::onCliVersionReady);
    m_manager->fetchCliVersion();

    // If the user enables/disables port forwarding while already connected,
    // start or stop the natpmpc loop immediately without waiting for reconnect.
    connect(m_manager, &VpnManager::configApplied, this, [this](const QString&)
    {
        if (m_currentState == VpnState::Connected)
        {
            if (m_manager->portForwardingEnabled() == true)
            {
                startNatPmpLoop();
            }
            else
            {
                stopNatPmpLoop();
                // Remove the "natpmpc not installed" banner if it is still visible.
                if (m_natpmpcBanner != nullptr)
                {
                    m_natpmpcBanner->deleteLater();
                    m_natpmpcBanner = nullptr;
                }
            }
        }
    });

    // Track the country code of the currently connected server so we can look
    // up city features via fetchCityFeatures() on startup.
    connect(m_manager, &VpnManager::connectionCountryKnown, this, [this](const QString& cc)
    {
        m_connectedCountryCode = cc;
    });

    //  NatPmpManager
    m_natPmpManager = new NatPmpManager(this);

    connect(m_natPmpManager, &NatPmpManager::portAcquired, this, [this](int port)
    {
        if (m_portLabel != nullptr)
        {
            m_portLabel->setText(QString::number(port));
        }
        if (m_portRow != nullptr)
        {
            m_portRow->setVisible(true);
        }
    });

    connect(m_natPmpManager, &NatPmpManager::portLost, this, [this]()
    {
        if (m_portRow != nullptr)
        {
            m_portRow->setVisible(false);
        }
    });

    connect(m_natPmpManager, &NatPmpManager::natpmpcMissing, this, [this]()
    {
        showNatpmpcBanner();
    });
    // Show a banner if this is a pre-release build
    checkPrereleaseBanner();
    checkFlatpakBetaBanner();
    checkAppImageBetaBanner();

    // React to plan type (Free vs Plus) - affects picker visibility and connect behaviour.
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

        if (m_hadUnknownConnection == false && city == m_activeCity)
            return;

        m_hadUnknownConnection = false;

        const QString locationName = city.isEmpty()
            ? tr("Fastest server")
            : city;

        QDialog* dlg = new QDialog(this);
        dlg->setWindowTitle(tr("Change Location?"));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setModal(true);
        dlg->setMinimumWidth(CHANGE_LOCATION_DLG_MIN_W);

        QVBoxLayout* layout = new QVBoxLayout(dlg);
        layout->setSpacing(DIALOG_SPACING);
        layout->setContentsMargins(DIALOG_H_MARGIN, DIALOG_H_MARGIN, DIALOG_H_MARGIN, DIALOG_BTM_MARGIN);

        QLabel* msgLabel = new QLabel(
            tr("You selected <b>%1</b>.<br>"
               "Would you like to connect to this location now, "
               "or use it on the next reconnect?").arg(locationName.toHtmlEscaped()),
            dlg);
        msgLabel->setWordWrap(true);
        msgLabel->setTextFormat(Qt::RichText);
        layout->addWidget(msgLabel);

        QHBoxLayout* btnRow = new QHBoxLayout();
        btnRow->setSpacing(DIALOG_BTN_SPACING);

        QPushButton* laterBtn = new QPushButton(tr("On next reconnect"), dlg);
        laterBtn->setObjectName(QStringLiteral("secondaryButton"));

        QPushButton* nowBtn = new QPushButton(tr("Connect now"), dlg);
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

    //  Sliding picker drawer (overlay - not part of outerLayout)
    // The drawer sits on the left edge of the VpnPage and overlays the main content.
    m_drawer = new PickerDrawer(m_locationPicker, m_recentPicker, m_favoritesPicker, this);
    m_drawer->setGeometry(0, 0, PickerDrawer::COLLAPSED_DRAWER_WIDTH, m_drawer->sizeHint().height());
    m_drawer->raise();

    // Notch toggle button - protrudes from the drawer's right edge
    m_drawerNotch = new QFrame(this);
    m_drawerNotch->setObjectName(QStringLiteral("drawerNotch"));
    m_drawerNotch->setFixedSize(DRAWER_NOTCH_W, DRAWER_NOTCH_H);
    m_drawerNotch->setCursor(Qt::PointingHandCursor);
    m_drawerNotch->setAttribute(Qt::WA_StyledBackground);

    QVBoxLayout* notchLayout = new QVBoxLayout(m_drawerNotch);
    notchLayout->setContentsMargins(4, 0, 4, 0);
    m_drawerNotchIcon = new QLabel(m_drawerNotch);
    m_drawerNotchIcon->setAlignment(Qt::AlignCenter);
    notchLayout->addWidget(m_drawerNotchIcon, 0, Qt::AlignCenter);
    updateDrawerNotchIcon();

    // Transparent click overlay so the whole notch frame is clickable
    QPushButton* notchBtn = new QPushButton(m_drawerNotch);
    notchBtn->setFlat(true);
    notchBtn->setGeometry(0, 0, DRAWER_NOTCH_W, DRAWER_NOTCH_BTN_H);
    notchBtn->setCursor(Qt::PointingHandCursor);
    notchBtn->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    connect(notchBtn, &QPushButton::clicked, this, [this]()
    {
        m_drawer->toggle();
        updateDrawerNotchIcon();
    });

    connect(m_drawer, &PickerDrawer::drawerWidthChanged, this, [this](int w)
    {
        repositionDrawerNotch(w);
    });

    connect(m_drawer, &PickerDrawer::pickerAvailabilityChanged, this, [this](bool hasAny)
    {
        if (m_wideMode == true)
        {
            if (m_pickerSidebar != nullptr)
            {
                m_pickerSidebar->setVisible(hasAny);
            }
        }
        else
        {
            m_drawer->setVisible(hasAny);
            m_drawerNotch->setVisible(hasAny);
            // Only offset the scroll area; logo/power rows stay full-width.
            const int leftMargin = hasAny ? PickerDrawer::COLLAPSED_DRAWER_WIDTH : 0;
            m_scrollOffsetLayout->setContentsMargins(leftMargin, 0, leftMargin, 0);
            repositionDrawer();
        }
    });

    repositionDrawerNotch(PickerDrawer::COLLAPSED_DRAWER_WIDTH);
    m_drawerNotch->raise();

    // Sync drawer with current config
    m_showFavoritesDropdown = AppConfig::instance().showFavoritesDropdown();
    relayoutPickers();
}

void VpnPage::notifyExternalConnect(const QString& city)
{
    m_activeCity = city;
    m_locationPicker->setSelectedCity(city);
    if (m_recentPicker != nullptr)
    {
        m_recentPicker->refresh();
    }
}

void VpnPage::onStatusCityKnown(const QString& city)
{
    // Store the city so updateUi() (triggered by the subsequent
    // connectionStateChanged signal) can skip the "Active connection" fallback,
    // and applyPendingStatusCity() can select it once the list is populated.
    m_pendingStatusCity = city;
    m_activeCity        = city;
    // If already connected (e.g. status monitor fires after the state was set),
    // apply the city immediately rather than waiting for a state transition.
    if (m_currentState == VpnState::Connected)
    {
        applyPendingStatusCity();
    }
}

void VpnPage::applyPendingStatusCity()
{
    if (m_pendingStatusCity.isEmpty())
        return;

    // If the city is in the local country's list the picker updates to show it.
    // If not (e.g. connected to a different country) the picker is left unchanged.
    m_locationPicker->trySelectCity(m_pendingStatusCity);
    m_pendingStatusCity.clear();
}

void VpnPage::refreshRecentPicker() const
{
    if (m_recentPicker != nullptr)
        m_recentPicker->refresh();
    relayoutPickers();
}

void VpnPage::refreshFavoritesPicker() const
{
    if (m_favoritesPicker != nullptr)
        m_favoritesPicker->refresh();
    relayoutPickers();
}

void VpnPage::setFavoritesDropdownVisible(const bool visible)
{
    m_showFavoritesDropdown = visible;
    relayoutPickers();
}

void VpnPage::setFavoritesEnabled(const bool enabled)
{
    Q_UNUSED(enabled);
    relayoutPickers();
}

void VpnPage::setLocationPickerVisible(const bool visible)
{
    if (m_locationPicker != nullptr)
    {
        m_locationPicker->setVisible(visible);
        if (m_drawer != nullptr)
            m_drawer->notifyAvailability();
    }
}

void VpnPage::relayoutPickers(int /*width*/) const
{
    if (m_drawer != nullptr)
    {
        m_drawer->syncVisibility(m_isFreeUser, m_showFavoritesDropdown,
                                 AppConfig::instance().favoritesEnabled());
    }
}

void VpnPage::repositionDrawer()
{
    if (m_drawer == nullptr)
        return;
    const int drawerH = m_drawer->sizeHint().height();
    m_drawer->setFixedHeight(drawerH);
    m_drawer->move(0, height() - drawerH);
    repositionDrawerNotch(m_drawer->width());
}

void VpnPage::applyWideMode(bool wide)
{
    if (m_wideMode == wide)
        return;
    m_wideMode = wide;

    if (wide == true)
    {
        //  Switch to wide mode
        // 1. Release pickers from the drawer and place them in the sidebar.
        //    Banners move to the sidebar above the pickers.
        m_drawer->releasePickers();
        if (m_vpnBannerArea != nullptr)
        {
            QVBoxLayout* sl = qobject_cast<QVBoxLayout*>(m_scrollArea->widget()->layout());
            if (sl != nullptr)
            {
                sl->removeWidget(m_vpnBannerArea);
            }
            // Wrap banner content in a scroll area for the sidebar (created once).
            if (m_vpnBannerContent != nullptr)
            {
                QVBoxLayout* bal = qobject_cast<QVBoxLayout*>(m_vpnBannerArea->layout());
                if (bal != nullptr)
                {
                    bal->removeWidget(m_vpnBannerContent);
                }
                if (m_vpnBannerScroll == nullptr)
                {
                    m_vpnBannerScroll = new QScrollArea(m_vpnBannerArea);
                    m_vpnBannerScroll->setWidgetResizable(true);
                    m_vpnBannerScroll->setFrameShape(QFrame::NoFrame);
                    m_vpnBannerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                    m_vpnBannerScroll->setMaximumHeight(BANNER_SCROLL_MAX_HEIGHT_WIDE);
                }
                m_vpnBannerScroll->setWidget(m_vpnBannerContent);
                if (bal != nullptr)
                {
                    bal->addWidget(m_vpnBannerScroll);
                }
            }
            // Insert at position 1: after the stretch at 0, before the pickers.
            m_pickerSidebarLayout->insertWidget(1, m_vpnBannerArea);
        }
        m_pickerSidebarLayout->addWidget(m_locationPicker, 0, Qt::AlignLeft);
        m_pickerSidebarLayout->addWidget(m_recentPicker, 0, Qt::AlignLeft);
        if (m_favoritesPicker != nullptr)
        {
            m_pickerSidebarLayout->addWidget(m_favoritesPicker, 0, Qt::AlignLeft);
        }
        // Ensure all pickers are in their expanded (full-size) form.
        // Note: isVisible() cannot be used here because the parent hierarchy
        // (m_wideContent) is still hidden at this point - it would always return
        // false, causing recent/favorites to stay collapsed.  Instead, expand
        // every picker unconditionally; hidden pickers will be given setCollapsed
        // again if they are ever re-shown via syncVisibility in narrow mode.
        m_locationPicker->setCollapsed(false);
        m_recentPicker->setCollapsed(false);
        if (m_favoritesPicker != nullptr)
        {
            m_favoritesPicker->setCollapsed(false);
        }

        // 2. Move topContentWidget and scrollArea into the right column.
        m_narrowContentLayout->removeWidget(m_topContentWidget);
        m_scrollOffsetLayout->removeWidget(m_scrollArea);
        m_rightContentLayout->addWidget(m_topContentWidget);
        m_rightContentLayout->addWidget(m_scrollArea, 1);

        // 3. Swap visible containers.
        m_narrowContent->setVisible(false);
        m_wideContent->setVisible(true);

        // 4. Hide the drawer overlay and notch.
        m_drawer->setVisible(false);
        m_drawerNotch->setVisible(false);

        // 5. Sidebar is shown only when at least one picker is available.
        m_pickerSidebar->setVisible(m_drawer->hasAnyVisiblePicker());
    }
    else
    {
        //  Switch to narrow mode
        // 1. Remove pickers from sidebar and reclaim them into the drawer.
        //    Banners move back to the bottom of the scroll content.
        m_pickerSidebarLayout->removeWidget(m_locationPicker);
        m_pickerSidebarLayout->removeWidget(m_recentPicker);
        if (m_favoritesPicker != nullptr)
        {
            m_pickerSidebarLayout->removeWidget(m_favoritesPicker);
        }
        m_drawer->reclaimPickers();
        // Restore collapsed state to match the current drawer state.
        // isVisible() must not be used here: the pickers were just reparented
        // to the hidden drawer, so isVisible() returns false for all of them.
        const bool collapsed = (m_drawer->isExpanded() == false);
        m_locationPicker->setCollapsed(collapsed);
        m_recentPicker->setCollapsed(collapsed);
        if (m_favoritesPicker != nullptr)
        {
            m_favoritesPicker->setCollapsed(collapsed);
        }

        // 2. Move topContentWidget and scrollArea back to the narrow wrapper.
        //    Return the banner area to the scroll content (before the stretch).
        m_rightContentLayout->removeWidget(m_topContentWidget);
        m_rightContentLayout->removeWidget(m_scrollArea);
        m_narrowContentLayout->insertWidget(0, m_topContentWidget);
        m_scrollOffsetLayout->addWidget(m_scrollArea, 1);
        if (m_vpnBannerArea != nullptr)
        {
            m_pickerSidebarLayout->removeWidget(m_vpnBannerArea);
            // Unwrap banner content from the scroll area back to direct placement.
            if (m_vpnBannerScroll != nullptr && m_vpnBannerContent != nullptr)
            {
                QVBoxLayout* bal = qobject_cast<QVBoxLayout*>(m_vpnBannerArea->layout());
                if (bal != nullptr)
                {
                    bal->removeWidget(m_vpnBannerScroll);
                }
                m_vpnBannerScroll->takeWidget();  // releases m_vpnBannerContent
                if (bal != nullptr)
                {
                    bal->addWidget(m_vpnBannerContent);
                }
            }
            QVBoxLayout* sl = qobject_cast<QVBoxLayout*>(m_scrollArea->widget()->layout());
            if (sl != nullptr)
            {
                sl->insertWidget(sl->count() - 1, m_vpnBannerArea);
            }
        }
        // Restore the scroll offset: only push right when the drawer is present.
        const int leftMargin = m_drawer->hasAnyVisiblePicker() ? PickerDrawer::COLLAPSED_DRAWER_WIDTH : 0;
        m_scrollOffsetLayout->setContentsMargins(leftMargin, 0, leftMargin, 0);

        // 3. Swap visible containers.
        m_wideContent->setVisible(false);
        m_narrowContent->setVisible(true);

        // 4. Restore drawer/notch/margin via the availability signal.
        m_drawer->notifyAvailability();
        repositionDrawer();
    }
}

void VpnPage::repositionDrawerNotch(int drawerW)
{
    if (m_drawerNotch == nullptr || m_drawer == nullptr)
        return;
    const int notchY = m_drawer->y() + (m_drawer->height() - m_drawerNotch->height()) / 2;
    m_drawerNotch->move(drawerW, notchY);
}

void VpnPage::updateDrawerNotchIcon()
{
    if (m_drawerNotchIcon == nullptr || m_drawer == nullptr) return;
    const QString path = m_drawer->isExpanded() == true
        ? QStringLiteral(":/assets/arrow-bar-left.svg")
        : QStringLiteral(":/assets/arrow-bar-right.svg");
    m_drawerNotchIcon->setPixmap(GeoUtils::svgPixmap(path, SMALL_ICON_PIX, NOTCH_ICON_COLOR));
}

void VpnPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    applyWideMode(event->size().width() >= kWideThreshold);
    if (m_wideMode == false && m_drawer != nullptr)
    {
        repositionDrawer();
    }
}

void VpnPage::onCitiesReady(const QString& countryCode,
                            const QList<QPair<QString, QString>>& cities)
{
    if (countryCode.compare(m_localCountryCode, Qt::CaseInsensitive) != 0)
        return;

    if (m_stateKnown == false)
    {
        m_pendingCities = cities; // wraps in optional - even an empty list is "received"
        return;
    }

    m_locationPicker->populate(cities);
    applyPendingStatusCity();

    // Update the tracked features for the currently active city so
    // refreshConnectedInfoLabel() can show "Port forwarding is active" when
    // the app starts with the VPN already connected to a P2P server.
    if (m_activeCity.isEmpty() == false)
    {
        const auto it = std::ranges::find_if(cities,
            [this](const QPair<QString, QString>& pair)
            {
                return pair.first.compare(m_activeCity, Qt::CaseInsensitive) == 0;
            });
        if (it != cities.end())
        {
            m_currentCityFeatures = it->second;
        }
    }
    if (m_currentState == VpnState::Connected)
    {
        refreshConnectedInfoLabel();
    }
}

void VpnPage::checkPrereleaseBanner()
{
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly) == false) return;

    const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
    vf.close();

    if (obj.value(QStringLiteral("prerelease")).toBool(false) == false) return;

    const QString appVersion = obj.value(QStringLiteral("app_version")).toString();
    const QString msg = tr(
        "You are running a <b>pre-release</b> version of this app (<b>v%1</b>). "
        "It may contain bugs or incomplete features. Use with caution.")
        .arg(appVersion.toHtmlEscaped());

    m_prereleaseBanner = new InfoBanner(msg, this);
    connect(m_prereleaseBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_prereleaseBanner = nullptr;
        updateBannerAreaVisibility();
    });
    m_vpnBannerLayout->addWidget(m_prereleaseBanner);
    updateBannerAreaVisibility();
}

void VpnPage::checkFlatpakBetaBanner()
{
    m_flatpakBetaBanner = FlatpakBetaBanner::createIfFlatpak(this);
    if (m_flatpakBetaBanner == nullptr) return;
    connect(m_flatpakBetaBanner, &FlatpakBetaBanner::dismissed, this, [this]()
    {
        m_flatpakBetaBanner = nullptr;
        updateBannerAreaVisibility();
    });
    m_vpnBannerLayout->addWidget(m_flatpakBetaBanner);
    updateBannerAreaVisibility();
}

void VpnPage::checkAppImageBetaBanner()
{
    m_appImageBetaBanner = AppImageBetaBanner::createIfAppImage(this);
    if (m_appImageBetaBanner == nullptr) return;
    connect(m_appImageBetaBanner, &AppImageBetaBanner::dismissed, this, [this]()
    {
        m_appImageBetaBanner = nullptr;
        updateBannerAreaVisibility();
    });
    m_vpnBannerLayout->addWidget(m_appImageBetaBanner);
    updateBannerAreaVisibility();
}

void VpnPage::updateBannerAreaVisibility()
{
    const int count = (m_prereleaseBanner   != nullptr ? 1 : 0)
                    + (m_flatpakBetaBanner  != nullptr ? 1 : 0)
                    + (m_appImageBetaBanner != nullptr ? 1 : 0);
    const bool hasAny = count > 0;
    m_vpnBannerArea->setVisible(hasAny);
    if (m_warningsHeaderLabel != nullptr)
    {
        m_warningsHeaderLabel->setText(tr("Warnings (%1)").arg(count));
    }
}

void VpnPage::onCliVersionReady(const QString& version)
{
    QString cliVersionMin;
    QString cliVersionMax;
    QFile vf(QStringLiteral(":/version.json"));
    if (vf.open(QIODevice::ReadOnly) == true)
    {
        const QJsonObject obj = QJsonDocument::fromJson(vf.readAll()).object();
        vf.close();
        cliVersionMin = obj.value(QStringLiteral("cli_version_tested_min")).toString();
        cliVersionMax = obj.value(QStringLiteral("cli_version_tested_max")).toString();
    }

    if (version.isEmpty() == true || (cliVersionMin.isEmpty() == true && cliVersionMax.isEmpty() == true)) return;

    const QVersionNumber installed = QVersionNumber::fromString(version);
    const QVersionNumber verMin    = QVersionNumber::fromString(cliVersionMin);
    const QVersionNumber verMax    = QVersionNumber::fromString(cliVersionMax);

    const bool tooOld = installed.isNull() == false && verMin.isNull() == false
                        && installed < verMin;
    const bool tooNew = installed.isNull() == false && verMax.isNull() == false
                        && installed > verMax;

    if (tooOld == false && tooNew == false) return;

    const QString rangeStr = (cliVersionMin.isEmpty() == false && cliVersionMax.isEmpty() == false)
        ? (cliVersionMin + QStringLiteral("-") + cliVersionMax)
        : (cliVersionMin.isEmpty() == true ? cliVersionMax : cliVersionMin);

    const QString msg = tooNew == true
        ? tr("Your Proton VPN CLI (<b>v%1</b>) is newer than the tested range (<b>%2</b>). "
             "Things may work fine, but you could encounter unexpected behavior.")
              .arg(version, rangeStr)
        : tr("Your Proton VPN CLI (<b>v%1</b>) is older than the tested range (<b>%2</b>). "
             "Some features may not work correctly. Consider upgrading the CLI.")
              .arg(version, rangeStr);

    const QScrollArea* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
    if (scrollArea == nullptr || scrollArea->widget() == nullptr) return;
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
    if (scrollLayout == nullptr) return;

    m_versionBanner = new InfoBanner(msg, this);
    connect(m_versionBanner, &InfoBanner::dismissed, this, [this]()
    {
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
    {
        m_checkingSpinnerTimer->stop();
    }

    const bool justBecameKnown = m_stateKnown == false && state != VpnState::Unknown;
    if (justBecameKnown == true)
    {
        m_stateKnown = true;
    }

    m_errorDetailsBtn->setVisible(false);
    m_signOutHintLabel->setVisible(false);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->style()->unpolish(m_infoLabel);
    m_infoLabel->style()->polish(m_infoLabel);
    m_infoLabel->setTextFormat(Qt::AutoText);
    m_infoLabel->setOpenExternalLinks(false);

    switch (state)
    {
    case VpnState::Connected:
        m_powerBtn->setState(PowerButton::RingState::Connected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(tr("Connected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #1a9c5b; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_lastConnectedInfo = info;
        refreshConnectedInfoLabel();
        startNatPmpLoop();

        // On app startup with VPN already connected, the CLI connect output is
        // not available, so "Port forwarding is active on this server." won't be
        // in `info`.  Fetch the city features explicitly and update the label once
        // we know whether it's a P2P server.
        if (m_manager->portForwardingEnabled() == true &&
            m_connectedCountryCode.isEmpty() == false && m_activeCity.isEmpty() == false)
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
            // Update the picker immediately using the city known at connect
            // time. The status monitor may not have polled yet, so
            // m_pendingStatusCity is often empty here; m_activeCity is always
            // set at the moment connectRequested was emitted.
            {
                const QString city = m_pendingStatusCity.isEmpty() == false
                    ? m_pendingStatusCity
                    : m_activeCity;
                m_locationPicker->trySelectCity(city);
                m_pendingStatusCity.clear();
            }
            if (m_isFreeUser == false && m_recentPicker != nullptr)
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
            if (prevState == VpnState::Unknown && m_activeCity.isEmpty() == true)
            {
                m_hadUnknownConnection = true;
                m_locationPicker->setUnknownConnection(true);
            }
        }
        if (justBecameKnown == true && m_pendingCities.has_value() == true)
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
        m_statusLabel->setText(tr("Disconnected"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(info.isEmpty() ? QString() : info);
        m_activeCity.clear();
        m_hadUnknownConnection = false;
        m_locationPicker->setUnknownConnection(false);
        stopElapsedTimer();
        if (justBecameKnown == true && m_pendingCities.has_value() == true)
        {
            m_locationPicker->populate(*m_pendingCities);
            m_pendingCities.reset();
        }
        break;

    case VpnState::Connecting:
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Spinning);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(tr("Connecting\u2026"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        m_infoLabel->setText(QString());
        stopElapsedTimer();
        break;

    case VpnState::Disconnecting:
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Spinning);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(tr("Disconnecting\u2026"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #f5a623; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        stopElapsedTimer();
        break;

    case VpnState::Error:
    {
        stopNatPmpLoop();
        m_powerBtn->setState(PowerButton::RingState::Disconnected);
        m_powerBtn->setEnabled(true);
        m_statusLabel->setText(tr("Error"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #d63f3f; font-size: 16pt; font-weight: bold; letter-spacing: 1px;"));
        stopElapsedTimer();

        m_rawError = info;

        const bool isCliError = info.contains(QLatin1String("Traceback (most recent call last)"))
                             || info.contains(QLatin1String("File \"/usr/bin/protonvpn\""))
                             || info.contains(QLatin1String("File \"/usr/lib/python"));
        if (isCliError == true)
        {
            m_infoLabel->setText(tr(
                "An error occurred in the Proton VPN CLI.\n"
                "Please file a bug report at "
                "<a href='https://github.com/ProtonVPN/proton-vpn-cli/issues'>github.com/ProtonVPN/proton-vpn-cli/</a>."));
            m_signOutHintLabel->setText(tr(
                "<span style='color:#f5a623;'>&#9888;</span>"
                " CLI errors can sometimes be resolved by "
                "<a href='action://signout' style='color:#ab8fff;'>signing out and signing back in</a>."));
            m_signOutHintLabel->setVisible(true);
        }
        else
        {
            m_infoLabel->setText(tr(
                "An error occurred in the ProtonVPN Qt desktop app.\n"
                "Please file a bug report at "
                "<a href='https://github.com/wheat32/proton-vpn-qt-app/issues'>github.com/wheat32/proton-vpn-qt-app</a>."));
        }
        m_infoLabel->setTextFormat(Qt::RichText);
        m_infoLabel->setOpenExternalLinks(true);
        m_infoLabel->setObjectName(QStringLiteral("errorLabel"));
        m_infoLabel->style()->unpolish(m_infoLabel);
        m_infoLabel->style()->polish(m_infoLabel);
        m_errorDetailsBtn->setVisible(info.trimmed().isEmpty() == false);
        break;
    }

    default: // Unknown
        m_powerBtn->setState(PowerButton::RingState::Unknown);
        m_powerBtn->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("\u280b Checking\u2026"));
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
    auto* dlg = new ErrorDetailsDialog(m_rawError, const_cast<VpnPage*>(this));
    dlg->exec();
}

void VpnPage::applyFreeUserMode() const
{
    // Location picker: block/unblock user interaction.
    m_locationPicker->setFreeMode(m_isFreeUser);

    // Re-sync drawer visibility (hides recent + favorites for free users).
    relayoutPickers();
}

// ---------------------------------------------------------------------------
// Port forwarding - NatPmpManager integration
// ---------------------------------------------------------------------------

void VpnPage::refreshConnectedInfoLabel() const
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
    const QString pfNote = tr("Port forwarding is active on this server.");
    if (pfEnabled == true && isP2P == true && text.contains(pfNote) == false)
    {
        if (text.isEmpty() == false)
        {
            text += QLatin1Char('\n');
        }
        text += pfNote;
    }

    m_infoLabel->setText(text);
}

void VpnPage::startNatPmpLoop()
{
    if (NatPmpManager::isInstalled() == false)
    {
        showNatpmpcBanner();
        return;
    }

    // natpmpc is available - dismiss any stale "not installed" banner that
    // may have been shown before the user installed the package at runtime.
    if (m_natpmpcBanner != nullptr)
    {
        m_natpmpcBanner->deleteLater();
        m_natpmpcBanner = nullptr;
    }

    // refresh() fires an immediate port-mapping request whether or not the
    // keep-alive loop was already running, preventing a 45-second wait.
    m_natPmpManager->refresh();
}

void VpnPage::showNatpmpcBanner()
{
    if (m_manager->portForwardingEnabled() == false)
        return; // port forwarding is off - don't nag the user
    if (m_natpmpcBanner != nullptr)
        return; // already showing

    const QScrollArea* scrollArea = findChild<QScrollArea*>(QStringLiteral("vpnScrollArea"));
    if (scrollArea == nullptr || scrollArea->widget() == nullptr) return;
    QVBoxLayout* scrollLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
    if (scrollLayout == nullptr) return;

    m_natpmpcBanner = new InfoBanner(
        tr("<b>natpmpc is not installed.</b> "
           "The forwarded port cannot be displayed or kept alive automatically. "
           "Install it to use port forwarding."),
        this);
    connect(m_natpmpcBanner, &InfoBanner::dismissed, this, [this]()
    {
        m_natpmpcBanner = nullptr;
    });
    int pos = 0;
    if (m_prereleaseBanner != nullptr) ++pos;
    if (m_versionBanner != nullptr)    ++pos;
    scrollLayout->insertWidget(pos, m_natpmpcBanner);
}

void VpnPage::stopNatPmpLoop()
{
    m_natPmpManager->stop();

    m_currentCityFeatures.clear();
    m_lastConnectedInfo.clear();
    m_connectedCountryCode.clear();

    if (m_portRow != nullptr)
    {
        m_portRow->setVisible(false);
    }
}

