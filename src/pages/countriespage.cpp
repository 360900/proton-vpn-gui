#include "countriespage.h"
#include "../geoutils.h"

#include <algorithm>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QSplitter>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

// ============================================================
// ElideLabel – a QLabel that elides its text with "…" at the
// right edge whenever it is too narrow to show it in full.
// ============================================================
class ElideLabel : public QLabel
{
public:
    explicit ElideLabel(const QString& text, QWidget* parent = nullptr)
        : QLabel(parent), m_fullText(text)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(0);
        QLabel::setText(elided());
    }

    // Override setText so callers can use it normally.
    void setText(const QString& text)
    {
        m_fullText = text;
        QLabel::setText(elided());
    }

protected:
    void resizeEvent(QResizeEvent* e) override
    {
        QLabel::resizeEvent(e);
        QLabel::setText(elided());
    }

private:
    QString elided() const
    {
        return fontMetrics().elidedText(m_fullText, Qt::ElideRight, width() > 0 ? width() : 9999);
    }

    QString m_fullText;
};

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

// City-level filter predicate used by both wide and narrow city lists.
static bool cityPassesFilters(const QString& features,
                              const bool filterP2P,
                              const bool filterSecureCore,
                              const bool filterTor)
{
    if (!filterP2P && !filterSecureCore && !filterTor)
        return true;

    // AND semantics: all enabled filters must be present.
    if (filterP2P && !hasFeature(features, QStringLiteral("p2p")))
        return false;
    if (filterSecureCore && !hasFeature(features, QStringLiteral("secure core")))
        return false;
    if (filterTor && !hasFeature(features, QStringLiteral("tor")))
        return false;
    return true;
}

// Shared star styling so wide and narrow views match exactly.
static void drawPinnedStar(QPainter& p, const QRect& r)
{
    QFont starFont = p.font();
    starFont.setBold(true);
    starFont.setPointSize(qMax(starFont.pointSize() + 1, 10));
    p.setFont(starFont);
    p.setPen(QColor(0xff, 0xd2, 0x4a)); // gold
    p.drawText(r, Qt::AlignCenter, QStringLiteral("★"));
}

// Wide country-list icon: optional star, then flag.
static QIcon makeCountryListIcon(const QString& countryCode, const bool pinned)
{
    // Keep a fixed icon box so wide-mode star + flag are never downscaled.
    const int iconW = 40;
    QPixmap pm(iconW, 16);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    int x = 12; // keep flag aligned across rows; star occupies the left slot
    if (pinned)
        drawPinnedStar(p, QRect(0, 0, 9, 16));

    const QPixmap flag = GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), 20, 15);
    if (!flag.isNull())
        p.drawPixmap(x, 0, flag);

    return QIcon(pm);
}

// Header icon for narrow accordion: arrow indicator, optional star, then country flag.
static QIcon makeAccordionHeaderIcon(const QString& countryCode,
                                     const bool expanded,
                                     const bool pinned)
{
    const int iconW = pinned ? 48 : 38;
    QPixmap pm(iconW, 16);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QString arrow = expanded ? QStringLiteral("▾") : QStringLiteral("▸");
    p.setPen(QColor(0xea, 0xea, 0xea));
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(qMax(f.pointSize() - 1, 8));
    p.setFont(f);
    p.drawText(QRect(0, 0, 10, 16), Qt::AlignCenter, arrow);

    int x = 12;
    if (pinned)
    {
        drawPinnedStar(p, QRect(x, 0, 9, 16));
        p.setFont(f);
        p.setPen(QColor(0xea, 0xea, 0xea));
        x += 12; // star width + right padding
    }

    const QPixmap flag = GeoUtils::svgPixmap(
        QStringLiteral(":/flags/") + countryCode.toLower(), 20, 15);
    if (!flag.isNull())
        p.drawPixmap(x, 0, flag);

    return QIcon(pm);
}

// Add a non-selectable, word-wrapped informational row to a QListWidget.
static void addWideInfoRow(QListWidget* list, const QString& text)
{
    if (!list) return;

    auto* item = new QListWidgetItem();
    item->setFlags(Qt::NoItemFlags);

    auto* row = new QWidget(list);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(10, 12, 10, 12);
    layout->setSpacing(0);

    auto* label = new QLabel(text, row);
    label->setObjectName(QStringLiteral("infoLabel"));
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(label);

    // Use a conservative wrapped-height estimate and a larger minimum to avoid
    // clipping with list item padding/borders in styled views.
    const int wrapW = qMax(200, list->viewport()->width() - 24);
    const QRect textRect = label->fontMetrics().boundingRect(
        QRect(0, 0, wrapW, 5000), Qt::TextWordWrap, text);
    const int rowH = qMax(56, textRect.height() + 36);
    item->setSizeHint(QSize(0, rowH));

    list->addItem(item);
    list->setItemWidget(item, row);
}

// ============================================================
// Bubble style helper
// ============================================================
QString CountriesPage::bubbleStyle(const bool active)
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
        if (m_portForwardingEnabled && m_filterP2P)
        {
            showDisablePortForwardingDialog(
                QStringLiteral("The <b>P2P</b> filter is active because <b>Port Forwarding</b> "
                                "is enabled in your settings.<br><br>"
                                "Clearing all filters will also remove the P2P filter, which "
                                "requires disabling Port Forwarding. Would you like to do that?"),
                [this]() {
                    m_portForwardingEnabled = false;
                    m_filterP2P = m_filterSecureCore = m_filterTor = false;
                    updateBubbleStyles();
                    applyFilter();
                });
            return;
        }

        m_filterP2P = m_filterSecureCore = m_filterTor = false;
        updateBubbleStyles();
        applyFilter();
    });
    connect(m_bubbleP2P, &QPushButton::clicked, this, [this]() {
        if (m_filterP2P && m_portForwardingEnabled)
        {
            showDisablePortForwardingDialog(
                QStringLiteral("The <b>P2P</b> filter is active because <b>Port Forwarding</b> "
                                "is enabled in your settings.<br><br>"
                                "To turn off this filter you need to disable Port Forwarding. "
                                "Would you like to do that?"),
                [this]() {
                    m_portForwardingEnabled = false;
                    m_filterP2P = false;
                    updateBubbleStyles();
                    applyFilter();
                });
            return;
        }

        // Normal toggle (port forwarding is not driving this filter)
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

    // ── Sync P2P filter with port forwarding setting ──────────────────────
    connect(m_manager, &VpnManager::settingsReady,
            this, [this](const QMap<QString, QString>& settings)
    {
        const QString v = settings.value(QStringLiteral("port-forwarding")).toLower().trimmed();
        const bool pfOn = (v == QLatin1String("on") || v == QLatin1String("true")
                           || v == QLatin1String("1") || v == QLatin1String("enabled"));
        m_portForwardingEnabled = pfOn;

        if (pfOn && !m_filterP2P)
        {
            m_filterP2P = true;
            updateBubbleStyles();
            applyFilter();
        }
        else if (!pfOn && m_filterP2P)
        {
            // Port forwarding was just turned off externally (e.g. from Settings page);
            // mirror that by clearing the P2P filter.
            m_filterP2P = false;
            updateBubbleStyles();
            applyFilter();
        }
    });

    // Re-read settings after any CLI config change so the P2P filter stays in
    // sync when the user toggles port forwarding from the Settings page.
    connect(m_manager, &VpnManager::configApplied,
            this, [this](const QString&)
    {
        m_manager->fetchSettings();
    });

    // Seed the initial port-forwarding state.
    m_manager->fetchSettings();

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
        const QString frame = QString::fromUtf8(frames[m_countriesSpinnerFrame]);
        if (m_countriesList && m_countriesList->count() > 0)
            m_countriesList->item(0)->setText(
                QStringLiteral("%1 Loading countries…").arg(frame));
        if (m_narrowLoadingLabel)
            m_narrowLoadingLabel->setText(
                QStringLiteral("%1 Loading countries…").arg(frame));
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

    // Initial loading state until countriesReady arrives.
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(QStringLiteral("Loading…"));

    if (m_countriesList)
    {
        m_countriesList->clear();
        m_countriesSpinnerFrame = 0;
        auto* loadingItem = new QListWidgetItem(QStringLiteral("⠋ Loading countries…"));
        loadingItem->setFlags(Qt::NoItemFlags);
        loadingItem->setForeground(QColor(0x99, 0x99, 0xbb));
        m_countriesList->addItem(loadingItem);
    }

    // While countries are loading, reset the right pane header.
    if (m_citiesLabel)
        m_citiesLabel->setText(QStringLiteral("Cities"));

    if (m_narrowLayout)
    {
        m_narrowLoadingLabel = new QLabel(QStringLiteral("⠋ Loading countries…"), m_narrowContent);
        m_narrowLoadingLabel->setObjectName(QStringLiteral("infoLabel"));
        m_narrowLoadingLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_narrowLoadingLabel->setContentsMargins(0, 10, 0, 0); // extra top padding
        m_narrowLayout->insertWidget(0, m_narrowLoadingLabel, 0, Qt::AlignHCenter);
    }

    m_countriesSpinnerTimer->start();
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
    // Wide mode uses a composite icon (star + flag) so reserve enough width.
    m_countriesList->setIconSize(QSize(40, 16));
    connect(m_countriesList, &QListWidget::itemClicked,
            this, &CountriesPage::onWideCountrySelected);
    countriesLayout->addWidget(m_countriesList);
    splitter->addWidget(countriesWidget);

    // Cities panel
    auto* citiesWidget = new QWidget(splitter);
    auto* citiesLayout = new QVBoxLayout(citiesWidget);
    citiesLayout->setContentsMargins(0, 0, 0, 0);
    citiesLayout->setSpacing(4);
    m_citiesLabel = new ElideLabel(QStringLiteral("Cities"), citiesWidget);
    m_citiesLabel->setObjectName(QStringLiteral("listHeader"));
    citiesLayout->addWidget(m_citiesLabel);
    m_citiesList = new QListWidget(citiesWidget);
    m_citiesList->setObjectName(QStringLiteral("serverList"));
    m_citiesList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

    // While countries are loading, reset the right pane header and list.
    if (m_citiesLabel)
        m_citiesLabel->setText(QStringLiteral("Cities"));
    if (m_citiesList) m_citiesList->clear();

    // Narrow: clear accordion
    while (m_narrowLayout->count() > 1)
    {
        auto* item = m_narrowLayout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    // Narrow: show loading text/spinner as well.
    if (!m_narrowLoadingLabel)
    {
        m_narrowLoadingLabel = new QLabel(QStringLiteral("⠋ Loading countries…"), m_narrowContent);
        m_narrowLoadingLabel->setObjectName(QStringLiteral("infoLabel"));
        m_narrowLoadingLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        m_narrowLoadingLabel->setContentsMargins(0, 10, 0, 0); // extra top padding
        m_narrowLayout->insertWidget(0, m_narrowLoadingLabel, 0, Qt::AlignHCenter);
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

    if (m_narrowLoadingLabel)
    {
        m_narrowLoadingLabel->deleteLater();
        m_narrowLoadingLabel = nullptr;
    }

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

    m_countriesLabel->setText(QStringLiteral("Countries (%1)").arg(matching.size()));

    if (m_narrowMode)
    {
        // If the selected country is no longer visible after filtering, clear its selection.
        if (!m_selectedCode.isEmpty() && !matching.isEmpty())
        {
            const bool stillVisible = std::any_of(matching.constBegin(), matching.constEnd(),
                [this](const QPair<QString,QString>& p){
                    return p.second.compare(m_selectedCode, Qt::CaseInsensitive) == 0;
                });
            if (!stillVisible)
            {
                m_selectedCode.clear();
                m_selectedCity.clear();
                m_selectedCountry.clear();
                if (auto* nbtn = qobject_cast<QPushButton*>(
                        m_narrowWidget->property("connectBtn").value<QObject*>()))
                {
                    nbtn->setEnabled(false);
                    nbtn->setText(QStringLiteral("Connect to Selected"));
                }
            }
        }
        populateNarrow();
        return;
    }

    // Wide mode: rebuild countries and then refresh/clear the city panel.
    populateWide();

    if (m_selectedCode.isEmpty())
    {
        if (m_citiesList)
        {
            m_citiesList->clear();
            addWideInfoRow(m_citiesList, QStringLiteral("Select a country to view cities."));
        }
        if (m_citiesLabel) m_citiesLabel->setText(QStringLiteral("Cities"));
        if (m_connectBtn)
        {
            m_connectBtn->setEnabled(false);
            m_connectBtn->setText(QStringLiteral("Connect to Selected"));
        }
        return;
    }

    QListWidgetItem* selectedItem = nullptr;
    if (m_countriesList)
    {
        for (int i = 0; i < m_countriesList->count(); ++i)
        {
            auto* item = m_countriesList->item(i);
            if (item && item->data(Qt::UserRole).toString() == m_selectedCode)
            {
                selectedItem = item;
                break;
            }
        }
    }

    if (!selectedItem)
    {
        // Keep the previously selected country context and show an empty-state
        // message instead of collapsing the header back to plain "Cities".
        m_selectedCity.clear();

        if (m_citiesList)
        {
            m_citiesList->clear();
            addWideInfoRow(m_citiesList,
                           QStringLiteral("No servers in this country match the selected filters."));
        }
        if (m_citiesLabel && !m_selectedCountry.isEmpty())
            m_citiesLabel->setText(QStringLiteral("Cities (0) – %1").arg(m_selectedCountry));
        if (m_connectBtn)
        {
            m_connectBtn->setEnabled(false);
            if (!m_selectedCountry.isEmpty())
                m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));
            else
                m_connectBtn->setText(QStringLiteral("Connect to Selected"));
        }
        return;
    }

    // Keep selection and refresh cities to reflect the active feature filters.
    m_countriesList->setCurrentItem(selectedItem);
    onWideCountrySelected(selectedItem);
}

// Country passes if at least one city satisfies all selected feature filters.
// If we have no city cache yet, keep it visible until data arrives.
bool CountriesPage::countryPassesFilter(const QString& code) const
{
    if (!m_filterP2P && !m_filterSecureCore && !m_filterTor)
        return true;

    if (!m_cityCache.contains(code))
        return true;

    const auto& cities = m_cityCache[code];
    for (const auto& cityPair : cities)
    {
        const QString& features = cityPair.second;
        if (cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor))
            return true;
    }
    return false;
}

// ============================================================
// showDisablePortForwardingDialog
// ============================================================
void CountriesPage::showDisablePortForwardingDialog(const QString& bodyText,
                                                     std::function<void()> onConfirm)
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QStringLiteral("Disable Port Forwarding?"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);

    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(16);
    layout->setContentsMargins(24, 24, 24, 20);

    auto* msg = new QLabel(bodyText, dlg);
    msg->setWordWrap(true);
    msg->setTextFormat(Qt::RichText);
    layout->addWidget(msg);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    auto* goBackBtn = new QPushButton(QStringLiteral("Go Back"), dlg);
    goBackBtn->setObjectName(QStringLiteral("secondaryButton"));

    auto* disableBtn = new QPushButton(QStringLiteral("Disable Port Forwarding"), dlg);
    disableBtn->setObjectName(QStringLiteral("primaryButton"));
    disableBtn->setDefault(true);

    // Match height to the primary button's natural height, same as the VPN page dialog.
    const int btnH = disableBtn->sizeHint().height();
    goBackBtn->setFixedHeight(btnH);
    disableBtn->setFixedHeight(btnH);

    // Stretch factor 1 on both so they share available width equally.
    btnRow->addWidget(goBackBtn, 1);
    btnRow->addWidget(disableBtn, 1);
    layout->addLayout(btnRow);

    // Ensure the dialog is wide enough that "Disable Port Forwarding" is never clipped.
    const int needed = disableBtn->sizeHint().width() * 2
                       + btnRow->spacing()
                       + layout->contentsMargins().left()
                       + layout->contentsMargins().right();
    dlg->setMinimumWidth(qMax(420, needed));

    connect(goBackBtn,  &QPushButton::clicked, dlg, &QDialog::reject);
    connect(disableBtn, &QPushButton::clicked, dlg, [this, dlg, onConfirm]() {
        m_manager->applyConfigValue(QStringLiteral("port-forwarding"),
                                    QStringLiteral("off"));
        onConfirm();
        dlg->accept();
    });

    dlg->exec();
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

        const bool isPinned = (!m_localCountryCode.isEmpty() &&
                               code.compare(m_localCountryCode, Qt::CaseInsensitive) == 0);

        auto* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, code);
        item->setData(Qt::UserRole + 10, isPinned);
        item->setIcon(makeCountryListIcon(code, isPinned));

        if (isPinned)
            pinnedItem = item;
        else
            others.append(item);
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

        const bool isPinned = (code.compare(pinnedCode, Qt::CaseInsensitive) == 0);
        const QString displayName = name;
        btn->setProperty("baseText", displayName);
        btn->setProperty("countryCode", code);
        btn->setProperty("isPinned", isPinned);
        btn->setText(displayName);
        btn->setIcon(makeAccordionHeaderIcon(code, false, isPinned));
        btn->setIconSize({isPinned ? 48 : 38, 16});

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
    if (anyFilter && code.compare(m_selectedCode, Qt::CaseInsensitive) != 0)
    {
        // Re-apply only for non-selected countries. Reapplying for the selected
        // country can recurse via applyFilter -> onWideCountrySelected -> onCitiesReady.
        applyFilter();
    }

    // ── Wide mode: fill cities list if this is the selected country ───────
    if (!m_narrowMode && code.compare(m_selectedCode, Qt::CaseInsensitive) == 0)
    {
        m_spinnerTimer->stop();
        if (!m_citiesList) return;
        m_citiesList->clear();

        const QString displayName = m_allCountries.key(code, code);

        int filteredCount = 0;
        for (const auto& [city, features] : cities)
        {
            if (!cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor))
                continue;
            ++filteredCount;
        }

        if (m_citiesLabel)
            m_citiesLabel->setText(QStringLiteral("Cities (%1) – %2")
                                   .arg(filteredCount).arg(displayName));

        if ((m_filterP2P || m_filterSecureCore || m_filterTor) && filteredCount == 0)
        {
            addWideInfoRow(m_citiesList,
                           QStringLiteral("No servers in this country match the selected filters."));
            return;
        }

        // "Fastest server" row (always available when not in a zero-match filtered state)
        auto* anyItem = new QListWidgetItem(QStringLiteral("⚡  Fastest server"));
        anyItem->setData(Qt::UserRole, QString());
        anyItem->setToolTip(QStringLiteral("Connects to the fastest server in this country."));
        anyItem->setForeground(QColor(0xab, 0x8f, 0xff));
        QFont f = m_citiesList->font(); f.setBold(true); f.setItalic(true);
        anyItem->setFont(f);
        anyItem->setBackground(QColor(0x6d, 0x4a, 0xff, 40));
        m_citiesList->addItem(anyItem);

        for (const auto& [city, features] : cities)
        {
            if (!cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor))
                continue;
            addWideCityItem(city, features);
        }
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

        // Count how many cities pass the filter first.
        int added = 0;
        for (const auto& [city, features] : cities)
        {
            if (!cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor))
                continue;
            ++added;
        }

        // "Fastest server" row — always shown unless ALL cities were filtered out.
        if (added > 0 || !(m_filterP2P || m_filterSecureCore || m_filterTor))
            addNarrowCityItem(acc.citiesLayout, QString(), QString(), code);

        // Now add the actual city rows.
        added = 0;
        for (const auto& [city, features] : cities)
        {
            if (!cityPassesFilters(features, m_filterP2P, m_filterSecureCore, m_filterTor))
                continue;
            addNarrowCityItem(acc.citiesLayout, city, features, code);
            ++added;
        }

        if ((m_filterP2P || m_filterSecureCore || m_filterTor) && added == 0)
        {
            auto* msg = new QLabel(QStringLiteral("No servers in this country match the selected filters."), acc.citiesWidget);
            msg->setObjectName(QStringLiteral("infoLabel"));
            msg->setWordWrap(true);
            acc.citiesLayout->addWidget(msg);
        }

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

    const bool isFastest = city.isEmpty();
    auto* cityLabel = new QLabel(isFastest ? QStringLiteral("⚡  Fastest server") : city, row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    if (isFastest)
    {
        QFont f = cityLabel->font();
        f.setBold(true);
        f.setItalic(true);
        cityLabel->setFont(f);
        cityLabel->setStyleSheet(QStringLiteral("color: #ab8fff;"));
        row->setStyleSheet(QStringLiteral("background-color: rgba(109, 74, 255, 40);"));
    }
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
    // Country name text is now star-free; keep as-is.
    const QString displayName = item->text();

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
        onCitiesReady(m_selectedCode, m_cityCache[m_selectedCode]);
    }
    else
    {
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

    const QString baseText = acc.headerBtn->property("baseText").toString();
    const QString headerCode = acc.headerBtn->property("countryCode").toString();
    const bool pinned = acc.headerBtn->property("isPinned").toBool();
    acc.headerBtn->setText(baseText);
    acc.headerBtn->setIcon(makeAccordionHeaderIcon(headerCode, acc.expanded, pinned));

    if (acc.expanded)
    {
        // Show immediately so loading text and then cities are visible.
        acc.citiesWidget->setVisible(true);
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
        {
            // Cached path (common for local country): populate now, because no
            // citiesReady signal will fire to fill this accordion section.
            while (acc.citiesLayout->count() > 0)
            {
                auto* li = acc.citiesLayout->takeAt(0);
                if (li->widget()) li->widget()->deleteLater();
                delete li;
            }
            // "Fastest server" row first.
            addNarrowCityItem(acc.citiesLayout, QString(), QString(), code);
            for (const auto& [city, features] : m_cityCache[code])
                addNarrowCityItem(acc.citiesLayout, city, features, code);
            acc.citiesWidget->setVisible(true);
        }
        // if not yet cached, onCitiesReady will fill the already-visible container
    }
    else
    {
        acc.citiesWidget->setVisible(false);
    }
}
