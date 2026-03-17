#include "countriespage.h"
#include "../geoutils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QSplitter>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

// ============================================================
// Feature metadata shared by wide + narrow city rows
// ============================================================
struct FeatureMeta { QString keyword; QString resource; QString tooltip; };
static const FeatureMeta kFeatures[] = {
    { QStringLiteral("p2p"),         QStringLiteral(":/assets/server-p2p.svg"),
      QStringLiteral("P2P — Optimized for peer-to-peer file sharing") },
    { QStringLiteral("secure core"), QStringLiteral(":/assets/server-secure-core.svg"),
      QStringLiteral("Secure Core — Routes through privacy-friendly countries") },
    { QStringLiteral("tor"),         QStringLiteral(":/assets/server-tor.svg"),
      QStringLiteral("Tor — Routes through the Tor anonymity network") },
};

// Helper: does a feature string contain a keyword?
static bool hasFeature(const QString& features, const QString& keyword)
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& t : tags)
        if (t.trimmed().contains(keyword, Qt::CaseInsensitive))
            return true;
    return false;
}

// ============================================================
// Bubble style helper
// ============================================================
QString CountriesPage::bubbleStyle(bool active)
{
    if (active)
        return QStringLiteral(
            "QPushButton { background: #6d4aff; color: white; border: 1px solid #6d4aff; "
            "border-radius: 12px; padding: 3px 12px; font-size: 12px; }"
            "QPushButton:hover { background: #7d5aff; }");
    return QStringLiteral(
        "QPushButton { background: transparent; color: #aaaacc; border: 1px solid #444466; "
        "border-radius: 12px; padding: 3px 12px; font-size: 12px; }"
        "QPushButton:hover { border-color: #6d4aff; color: #ccccee; }");
}

// ============================================================
// Constructor
// ============================================================
CountriesPage::CountriesPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager),
      m_localCountryCode(GeoUtils::detectUserCountry())
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // ── Header row ───────────────────────────────────────────────────────
    auto* headerRow = new QHBoxLayout();

    m_countriesLabel = new QLabel(QStringLiteral("Countries"), this);
    m_countriesLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(m_countriesLabel);
    headerRow->addStretch();

    m_refreshBtn = new QPushButton(QStringLiteral("↻ Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(30);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CountriesPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    mainLayout->addLayout(headerRow);

    // ── Search ───────────────────────────────────────────────────────────
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("inputField"));
    m_searchEdit->setPlaceholderText(QStringLiteral("Search countries…"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&){ applyFilter(); });
    mainLayout->addWidget(m_searchEdit);

    // ── Filter bubbles ────────────────────────────────────────────────────
    auto* bubblesRow = new QHBoxLayout();
    bubblesRow->setSpacing(6);

    auto makeBubble = [&](const QString& label) -> QPushButton* {
        auto* btn = new QPushButton(label, this);
        btn->setCursor(Qt::PointingHandCursor);
        return btn;
    };

    m_bubbleAll        = makeBubble(QStringLiteral("All"));
    m_bubbleP2P        = makeBubble(QStringLiteral("P2P"));
    m_bubbleSecureCore = makeBubble(QStringLiteral("Secure Core"));
    m_bubbleTor        = makeBubble(QStringLiteral("Tor"));

    bubblesRow->addWidget(m_bubbleAll);
    bubblesRow->addWidget(m_bubbleP2P);
    bubblesRow->addWidget(m_bubbleSecureCore);
    bubblesRow->addWidget(m_bubbleTor);
    bubblesRow->addStretch();
    mainLayout->addLayout(bubblesRow);

    updateBubbleStyles();

    connect(m_bubbleAll, &QPushButton::clicked, this, [this]() {
        m_filterP2P = m_filterSecureCore = m_filterTor = false;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleP2P, &QPushButton::clicked, this, [this]() {
        m_filterP2P = !m_filterP2P;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleSecureCore, &QPushButton::clicked, this, [this]() {
        m_filterSecureCore = !m_filterSecureCore;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleTor, &QPushButton::clicked, this, [this]() {
        m_filterTor = !m_filterTor;
        updateBubbleStyles();
        applyFilter();
    });

    // ── Wide / narrow container ───────────────────────────────────────────
    buildWideLayout(mainLayout);
    buildNarrowLayout(mainLayout);

    // ── Spinners ──────────────────────────────────────────────────────────
    static constexpr const char* frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    static constexpr int frameCount = 10;

    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]() {
        m_spinnerFrame = (m_spinnerFrame + 1) % frameCount;
        if (m_citiesList && m_citiesList->count() > 0)
            m_citiesList->item(0)->setText(
                QStringLiteral("%1 Loading cities…").arg(QString::fromUtf8(frames[m_spinnerFrame])));
    });

    m_countriesSpinnerTimer = new QTimer(this);
    m_countriesSpinnerTimer->setInterval(200);
    connect(m_countriesSpinnerTimer, &QTimer::timeout, this, [this]() {
        m_countriesSpinnerFrame = (m_countriesSpinnerFrame + 1) % frameCount;
        if (m_countriesList && m_countriesList->count() > 0)
            m_countriesList->item(0)->setText(
                QStringLiteral("%1 Loading countries…").arg(QString::fromUtf8(frames[m_countriesSpinnerFrame])));
    });

    // ── VpnManager signals ────────────────────────────────────────────────
    connect(m_manager, &VpnManager::countriesReady,
            this, &CountriesPage::onCountriesReady);
    connect(m_manager, &VpnManager::citiesReady, this,
            [this](const QString& code, const QList<QPair<QString,QString>>& cities) {
                onCitiesReady(code, cities);
            });

    // Determine initial layout mode based on current width
    m_narrowMode = width() < kNarrowThreshold;
    m_wideWidget->setVisible(!m_narrowMode);
    m_narrowWidget->setVisible(m_narrowMode);
}

// ============================================================
// buildWideLayout
// ============================================================
void CountriesPage::buildWideLayout(QVBoxLayout* parent)
{
    m_wideWidget = new QWidget(this);
    auto* wideLayout = new QVBoxLayout(m_wideWidget);
    wideLayout->setContentsMargins(0, 0, 0, 0);
    wideLayout->setSpacing(8);

    auto* splitter = new QSplitter(Qt::Horizontal, m_wideWidget);

    // Countries panel
    auto* countriesWidget = new QWidget(splitter);
    auto* countriesLayout = new QVBoxLayout(countriesWidget);
    countriesLayout->setContentsMargins(0, 0, 0, 0);
    m_countriesList = new QListWidget(countriesWidget);
    m_countriesList->setObjectName(QStringLiteral("serverList"));
    m_countriesList->setIconSize(QSize(20, 15));
    connect(m_countriesList, &QListWidget::itemClicked,
            this, &CountriesPage::onWideCountrySelected);
    countriesLayout->addWidget(m_countriesList);
    splitter->addWidget(countriesWidget);

    // Cities panel
    auto* citiesWidget = new QWidget(splitter);
    auto* citiesLayout = new QVBoxLayout(citiesWidget);
    citiesLayout->setContentsMargins(0, 0, 0, 0);
    citiesLayout->setSpacing(4);
    m_citiesLabel = new QLabel(QStringLiteral("Cities"), citiesWidget);
    m_citiesLabel->setObjectName(QStringLiteral("listHeader"));
    citiesLayout->addWidget(m_citiesLabel);
    m_citiesList = new QListWidget(citiesWidget);
    m_citiesList->setObjectName(QStringLiteral("serverList"));
    connect(m_citiesList, &QListWidget::itemClicked,
            this, &CountriesPage::onWideCitySelected);
    citiesLayout->addWidget(m_citiesList);
    splitter->addWidget(citiesWidget);

    splitter->setSizes({200, 200});
    wideLayout->addWidget(splitter, 1);

    // Connect button
    m_connectBtn = new QPushButton(QStringLiteral("Connect to Selected"), m_wideWidget);
    m_connectBtn->setObjectName(QStringLiteral("primaryButton"));
    m_connectBtn->setEnabled(false);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        emit connectRequested(m_selectedCode, m_selectedCity);
    });
    wideLayout->addWidget(m_connectBtn);

    parent->addWidget(m_wideWidget, 1);
}

// ============================================================
// buildNarrowLayout
// ============================================================
void CountriesPage::buildNarrowLayout(QVBoxLayout* parent)
{
    m_narrowWidget = new QWidget(this);
    auto* narrowOuterLayout = new QVBoxLayout(m_narrowWidget);
    narrowOuterLayout->setContentsMargins(0, 0, 0, 0);
    narrowOuterLayout->setSpacing(0);

    m_narrowScroll = new QScrollArea(m_narrowWidget);
    m_narrowScroll->setWidgetResizable(true);
    m_narrowScroll->setFrameShape(QFrame::NoFrame);

    m_narrowContent = new QWidget();
    m_narrowLayout = new QVBoxLayout(m_narrowContent);
    m_narrowLayout->setContentsMargins(0, 0, 0, 0);
    m_narrowLayout->setSpacing(0);
    m_narrowLayout->addStretch();

    m_narrowScroll->setWidget(m_narrowContent);
    narrowOuterLayout->addWidget(m_narrowScroll, 1);

    // Connect button for narrow mode
    auto* narrowConnectBtn = new QPushButton(QStringLiteral("Connect to Selected"), m_narrowWidget);
    narrowConnectBtn->setObjectName(QStringLiteral("primaryButton"));
    narrowConnectBtn->setEnabled(false);
    narrowConnectBtn->setCursor(Qt::PointingHandCursor);
    // Share the same enabled/text state with the wide connect button by re-using m_connectBtn
    // (we keep them in sync manually)
    connect(narrowConnectBtn, &QPushButton::clicked, this, [this]() {
        emit connectRequested(m_selectedCode, m_selectedCity);
    });
    // Store as the narrow connect button — we'll sync text/enabled with m_connectBtn
    narrowOuterLayout->addWidget(narrowConnectBtn);

    // Keep the narrow connect button pointer for sync
    narrowConnectBtn->setObjectName(QStringLiteral("primaryButton"));
    m_narrowWidget->setProperty("connectBtn", QVariant::fromValue(static_cast<QObject*>(narrowConnectBtn)));

    parent->addWidget(m_narrowWidget, 1);
}

// ============================================================
// resizeEvent – switch layout at threshold
// ============================================================
void CountriesPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    const bool shouldBeNarrow = event->size().width() < kNarrowThreshold;
    if (shouldBeNarrow != m_narrowMode)
        switchLayout(shouldBeNarrow);
}

void CountriesPage::switchLayout(bool narrow)
{
    m_narrowMode = narrow;
    m_wideWidget->setVisible(!narrow);
    m_narrowWidget->setVisible(narrow);

    // Re-populate whichever view just became visible
    if (!m_allCountries.isEmpty())
    {
        if (narrow)
            populateNarrow();
        else
            populateWide();
    }
}

// ============================================================
// refresh
// ============================================================
void CountriesPage::refresh()
{
    m_cityCache.clear();
    m_pendingCityCodes.clear();
    m_accordion.clear();
    m_selectedCode.clear();
    m_selectedCity.clear();
    m_selectedCountry.clear();

    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(QStringLiteral("Loading…"));

    if (m_connectBtn) m_connectBtn->setEnabled(false);

    // Wide: show spinner
    if (m_countriesList)
    {
        m_countriesList->clear();
        m_countriesSpinnerFrame = 0;
        auto* item = new QListWidgetItem(QStringLiteral("⠋ Loading countries…"));
        item->setFlags(Qt::NoItemFlags);
        item->setForeground(QColor(0x99, 0x99, 0xbb));
        m_countriesList->addItem(item);
        m_countriesSpinnerTimer->start();
    }
    if (m_citiesList) m_citiesList->clear();

    // Narrow: clear accordion
    while (m_narrowLayout->count() > 1)
    {
        auto* item = m_narrowLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    m_manager->fetchCountries();
}

// ============================================================
// onCountriesReady
// ============================================================
void CountriesPage::onCountriesReady(const QMap<QString, QString>& countries)
{
    m_countriesSpinnerTimer->stop();
    m_allCountries = countries;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(QStringLiteral("↻ Refresh"));

    applyFilter();

    // Auto-select local country if detected
    if (!m_localCountryCode.isEmpty() && !m_narrowMode && m_countriesList)
    {
        for (int i = 0; i < m_countriesList->count(); ++i)
        {
            auto* item = m_countriesList->item(i);
            if (item && item->data(Qt::UserRole).toString().compare(
                    m_localCountryCode, Qt::CaseInsensitive) == 0)
            {
                m_countriesList->setCurrentItem(item);
                onWideCountrySelected(item);
                break;
            }
        }
    }
}

// ============================================================
// applyFilter – rebuild the visible country lists
// ============================================================
void CountriesPage::applyFilter()
{
    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    const QString search = m_searchEdit ? m_searchEdit->text() : QString();

    // For feature-based filtering we need city data.
    // Collect matching country codes.
    QList<QPair<QString,QString>> matching; // {name, code}
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QString& code = it.value();
        if (!search.isEmpty() && !name.contains(search, Qt::CaseInsensitive))
            continue;
        if (anyFilter && !countryPassesFilter(code))
            continue;
        matching.append({name, code});
    }

    const int total = matching.size();
    m_countriesLabel->setText(QStringLiteral("Countries (%1)").arg(total));

    if (m_narrowMode)
        populateNarrow();
    else
        populateWide();
}

// countryPassesFilter: returns true if we have city data with the required feature,
// or if we haven't fetched cities yet (we can't know — show it optimistically).
bool CountriesPage::countryPassesFilter(const QString& code) const
{
    if (!m_filterP2P && !m_filterSecureCore && !m_filterTor)
        return true;

    if (!m_cityCache.contains(code))
        return true; // not fetched yet — show optimistically

    const auto& cities = m_cityCache[code];
    for (const auto& [city, features] : cities)
    {
        if (m_filterP2P        && hasFeature(features, QStringLiteral("p2p")))         return true;
        if (m_filterSecureCore && hasFeature(features, QStringLiteral("secure core"))) return true;
        if (m_filterTor        && hasFeature(features, QStringLiteral("tor")))         return true;
    }
    return false;
}

void CountriesPage::updateBubbleStyles()
{
    const bool anyActive = m_filterP2P || m_filterSecureCore || m_filterTor;
    m_bubbleAll->setStyleSheet(bubbleStyle(!anyActive));
    m_bubbleP2P->setStyleSheet(bubbleStyle(m_filterP2P));
    m_bubbleSecureCore->setStyleSheet(bubbleStyle(m_filterSecureCore));
    m_bubbleTor->setStyleSheet(bubbleStyle(m_filterTor));
}

// ============================================================
// populateWide – rebuild the countries QListWidget
// ============================================================
void CountriesPage::populateWide()
{
    if (!m_countriesList) return;
    m_countriesList->clear();

    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    const QString search = m_searchEdit ? m_searchEdit->text() : QString();

    QListWidgetItem* pinnedItem = nullptr;
    QList<QListWidgetItem*> others;

    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QString& code = it.value();
        if (!search.isEmpty() && !name.contains(search, Qt::CaseInsensitive))
            continue;
        if (anyFilter && !countryPassesFilter(code))
            continue;

        auto* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, code);
        const QIcon icon = GeoUtils::flagIcon(code);
        if (!icon.isNull()) item->setIcon(icon);

        if (!m_localCountryCode.isEmpty() &&
            code.compare(m_localCountryCode, Qt::CaseInsensitive) == 0)
        {
            item->setText(QStringLiteral("★ %1").arg(name));
            pinnedItem = item;
        }
        else
        {
            others.append(item);
        }
    }

    if (pinnedItem) m_countriesList->addItem(pinnedItem);
    for (auto* it : others) m_countriesList->addItem(it);
}

// ============================================================
// populateNarrow – rebuild accordion
// ============================================================
void CountriesPage::populateNarrow()
{
    // Remove all except the trailing stretch
    while (m_narrowLayout->count() > 1)
    {
        auto* layoutItem = m_narrowLayout->takeAt(0);
        if (layoutItem->widget()) layoutItem->widget()->deleteLater();
        delete layoutItem;
    }
    m_accordion.clear();

    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    const QString search = m_searchEdit ? m_searchEdit->text() : QString();

    // Pinned country first
    QString pinnedCode;
    if (!m_localCountryCode.isEmpty())
        pinnedCode = m_localCountryCode.toUpper();

    auto addAccordion = [&](const QString& name, const QString& code) {
        auto* container = new QWidget(m_narrowContent);
        auto* containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // Header button
        auto* btn = new QToolButton(container);
        btn->setObjectName(QStringLiteral("accordionHeader"));
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(42);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setArrowType(Qt::RightArrow);

        const QString displayName = (code.compare(pinnedCode, Qt::CaseInsensitive) == 0)
            ? QStringLiteral("★ %1").arg(name) : name;
        btn->setText(displayName);

        const QIcon icon = GeoUtils::flagIcon(code);
        if (!icon.isNull()) { btn->setIcon(icon); btn->setIconSize({20, 15}); }

        containerLayout->addWidget(btn);

        // Cities area (hidden initially)
        auto* citiesWidget = new QWidget(container);
        citiesWidget->setVisible(false);
        auto* citiesLayout = new QVBoxLayout(citiesWidget);
        citiesLayout->setContentsMargins(24, 0, 0, 0);
        citiesLayout->setSpacing(0);
        containerLayout->addWidget(citiesWidget);

        AccordionItem acc;
        acc.headerBtn    = btn;
        acc.citiesWidget = citiesWidget;
        acc.citiesLayout = citiesLayout;
        acc.expanded     = false;
        m_accordion.insert(code, acc);

        connect(btn, &QToolButton::clicked, this, [this, code]() {
            toggleAccordion(code);
        });

        // Insert before the stretch
        m_narrowLayout->insertWidget(m_narrowLayout->count() - 1, container);
    };

    // Insert pinned first
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        if (it.value().compare(pinnedCode, Qt::CaseInsensitive) != 0) continue;
        if (!search.isEmpty() && !it.key().contains(search, Qt::CaseInsensitive)) continue;
        if (anyFilter && !countryPassesFilter(it.value())) continue;
        addAccordion(it.key(), it.value());
    }
    // Then the rest alphabetically
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        if (it.value().compare(pinnedCode, Qt::CaseInsensitive) == 0) continue;
        if (!search.isEmpty() && !it.key().contains(search, Qt::CaseInsensitive)) continue;
        if (anyFilter && !countryPassesFilter(it.value())) continue;
        addAccordion(it.key(), it.value());
    }
}

// ============================================================
// onCitiesReady
// ============================================================
void CountriesPage::onCitiesReady(const QString& code,
                                   const QList<QPair<QString,QString>>& cities)
{
    m_cityCache.insert(code, cities);
    m_pendingCityCodes.remove(code);

    const bool anyFilter = m_filterP2P || m_filterSecureCore || m_filterTor;
    if (anyFilter)
    {
        // Now we have city data — re-apply filter (might remove this country)
        applyFilter();
    }

    // ── Wide mode: fill cities list if this is the selected country ───────
    if (!m_narrowMode && code.compare(m_selectedCode, Qt::CaseInsensitive) == 0)
    {
        m_spinnerTimer->stop();
        if (!m_citiesList) return;
        m_citiesList->clear();

        const QString displayName = m_allCountries.key(code, code);
        if (m_citiesLabel)
            m_citiesLabel->setText(QStringLiteral("Cities (%1) – %2")
                                   .arg(cities.size()).arg(displayName));

        // "Fastest server" row
        auto* anyItem = new QListWidgetItem(QStringLiteral("⚡  Fastest server"));
        anyItem->setData(Qt::UserRole, QString());
        anyItem->setToolTip(QStringLiteral("Connects to the fastest server in this country."));
        anyItem->setForeground(QColor(0xab, 0x8f, 0xff));
        QFont f = m_citiesList->font(); f.setBold(true); f.setItalic(true);
        anyItem->setFont(f);
        anyItem->setBackground(QColor(0x6d, 0x4a, 0xff, 40));
        m_citiesList->addItem(anyItem);

        for (const auto& [city, features] : cities)
            addWideCityItem(city, features);
    }

    // ── Narrow mode: fill the accordion for this country ──────────────────
    if (m_narrowMode && m_accordion.contains(code))
    {
        auto& acc = m_accordion[code];
        // Clear old placeholder
        while (acc.citiesLayout->count() > 0)
        {
            auto* li = acc.citiesLayout->takeAt(0);
            if (li->widget()) li->widget()->deleteLater();
            delete li;
        }

        // Add city rows
        for (const auto& [city, features] : cities)
            addNarrowCityItem(acc.citiesLayout, city, features, code);

        if (acc.expanded)
            acc.citiesWidget->setVisible(true);
    }
}

// ============================================================
// Wide city item
// ============================================================
void CountriesPage::addWideCityItem(const QString& city, const QString& features)
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);

    auto* row = new QWidget();
    row->setAttribute(Qt::WA_TranslucentBackground);
    auto* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(6);

    auto* cityLabel = new QLabel(city, row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    hbox->addWidget(cityLabel, 0, Qt::AlignVCenter);
    hbox->addStretch();

    for (const auto& meta : kFeatures)
    {
        bool matched = false;
        for (const QString& tag : tags)
            if (tag.trimmed().contains(meta.keyword, Qt::CaseInsensitive))
                { matched = true; break; }
        if (!matched) continue;

        auto* iconLabel = new QLabel(row);
        iconLabel->setPixmap(GeoUtils::svgPixmap(meta.resource, 16));
        iconLabel->setFixedSize(24, 24);
        iconLabel->setScaledContents(false);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(meta.tooltip);
        hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    int itemH = -1;
    if (m_countriesList && m_countriesList->count() > 0)
        itemH = m_countriesList->sizeHintForRow(0);
    if (itemH <= 0 && m_citiesList && m_citiesList->count() > 0)
        itemH = m_citiesList->sizeHintForRow(0);
    if (itemH <= 0)
        itemH = (m_countriesList ? m_countriesList->fontMetrics().height() : 14) + 17;

    auto* item = new QListWidgetItem();
    item->setData(Qt::UserRole, city);
    item->setSizeHint(QSize(0, itemH));
    m_citiesList->addItem(item);
    m_citiesList->setItemWidget(item, row);
}

// ============================================================
// Narrow city item
// ============================================================
void CountriesPage::addNarrowCityItem(QVBoxLayout* layout, const QString& city,
                                       const QString& features, const QString& code)
{
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);

    auto* row = new QWidget();
    row->setCursor(Qt::PointingHandCursor);
    auto* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(8, 6, 8, 6);
    hbox->setSpacing(6);

    auto* cityLabel = new QLabel(city.isEmpty() ? QStringLiteral("⚡  Fastest server") : city, row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    hbox->addWidget(cityLabel, 1, Qt::AlignVCenter);

    for (const auto& meta : kFeatures)
    {
        bool matched = false;
        for (const QString& tag : tags)
            if (tag.trimmed().contains(meta.keyword, Qt::CaseInsensitive))
                { matched = true; break; }
        if (!matched) continue;

        auto* iconLabel = new QLabel(row);
        iconLabel->setPixmap(GeoUtils::svgPixmap(meta.resource, 14));
        iconLabel->setFixedSize(20, 20);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(meta.tooltip);
        hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    // Connect button
    auto* connectBtn = new QPushButton(QStringLiteral("Connect"), row);
    connectBtn->setObjectName(QStringLiteral("secondaryButton"));
    connectBtn->setFixedHeight(26);
    connectBtn->setCursor(Qt::PointingHandCursor);
    connect(connectBtn, &QPushButton::clicked, this, [this, code, city]() {
        emit connectRequested(code, city);
    });
    hbox->addWidget(connectBtn, 0, Qt::AlignVCenter);

    layout->addWidget(row);

    // Divider
    auto* div = new QFrame();
    div->setFrameShape(QFrame::HLine);
    div->setObjectName(QStringLiteral("divider"));
    layout->addWidget(div);
}

// ============================================================
// Wide country selected
// ============================================================
void CountriesPage::onWideCountrySelected(QListWidgetItem* item)
{
    QString displayName = item->text();
    if (displayName.startsWith(QStringLiteral("★ ")))
        displayName = displayName.mid(2);

    m_selectedCountry = displayName;
    m_selectedCode    = item->data(Qt::UserRole).toString();
    m_selectedCity.clear();

    if (m_citiesList) m_citiesList->clear();
    if (m_citiesLabel) m_citiesLabel->setText(QStringLiteral("Cities – %1").arg(m_selectedCountry));
    if (m_connectBtn)
    {
        m_connectBtn->setEnabled(!m_selectedCode.isEmpty());
        m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));
    }

    if (m_selectedCode.isEmpty()) return;

    if (m_cityCache.contains(m_selectedCode))
    {
        // Already cached — emit a synthetic signal
        onCitiesReady(m_selectedCode, m_cityCache[m_selectedCode]);
    }
    else
    {
        // Show spinner while fetching
        m_spinnerFrame = 0;
        auto* li = new QListWidgetItem(QStringLiteral("⠋ Loading cities…"));
        li->setFlags(Qt::NoItemFlags);
        li->setForeground(QColor(0x99, 0x99, 0xbb));
        if (m_citiesList) m_citiesList->addItem(li);
        m_spinnerTimer->start();
        m_manager->fetchCities(m_selectedCode);
    }
}

void CountriesPage::onWideCitySelected(QListWidgetItem* item)
{
    m_selectedCity = item->data(Qt::UserRole).toString();
    if (!m_connectBtn) return;
    if (m_selectedCity.isEmpty())
        m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));
    else
        m_connectBtn->setText(QStringLiteral("Connect to %1, %2")
                              .arg(m_selectedCountry, m_selectedCity));
}

// ============================================================
// Accordion helpers
// ============================================================
void CountriesPage::ensureCities(const QString& code)
{
    if (m_cityCache.contains(code) || m_pendingCityCodes.contains(code))
        return;
    m_pendingCityCodes.insert(code);
    m_manager->fetchCities(code);

    // Show a loading placeholder in the narrow accordion
    if (m_accordion.contains(code))
    {
        auto& acc = m_accordion[code];
        auto* li = new QLabel(QStringLiteral("⠋ Loading cities…"), acc.citiesWidget);
        li->setObjectName(QStringLiteral("infoLabel"));
        acc.citiesLayout->addWidget(li);
    }
}

void CountriesPage::toggleAccordion(const QString& code)
{
    if (!m_accordion.contains(code)) return;
    auto& acc = m_accordion[code];
    acc.expanded = !acc.expanded;
    acc.headerBtn->setArrowType(acc.expanded ? Qt::DownArrow : Qt::RightArrow);

    if (acc.expanded)
    {
        ensureCities(code);

        // Update selection state
        m_selectedCode = code;
        m_selectedCity.clear();
        m_selectedCountry = m_allCountries.key(code, code);

        // Sync narrow connect button
        if (auto* nbtn = qobject_cast<QPushButton*>(
                m_narrowWidget->property("connectBtn").value<QObject*>()))
        {
            nbtn->setEnabled(true);
            nbtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));
        }

        if (m_cityCache.contains(code))
            acc.citiesWidget->setVisible(true);
        // if not yet cached, onCitiesReady will make it visible
    }
    else
    {
        acc.citiesWidget->setVisible(false);
    }
}
