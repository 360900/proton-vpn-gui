#include "countriespage.h"

#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QLocale>
#include <QTimeZone>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QToolButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Timezone → country-code mapping (representative/most-common zone per country)
// ---------------------------------------------------------------------------
static const QMap<QString, QString> kTimezoneToCountry = {
    // Americas
    {QStringLiteral("America/New_York"),        QStringLiteral("US")},
    {QStringLiteral("America/Chicago"),         QStringLiteral("US")},
    {QStringLiteral("America/Denver"),          QStringLiteral("US")},
    {QStringLiteral("America/Los_Angeles"),     QStringLiteral("US")},
    {QStringLiteral("America/Anchorage"),       QStringLiteral("US")},
    {QStringLiteral("Pacific/Honolulu"),        QStringLiteral("US")},
    {QStringLiteral("America/Phoenix"),         QStringLiteral("US")},
    {QStringLiteral("America/Toronto"),         QStringLiteral("CA")},
    {QStringLiteral("America/Vancouver"),       QStringLiteral("CA")},
    {QStringLiteral("America/Winnipeg"),        QStringLiteral("CA")},
    {QStringLiteral("America/Halifax"),         QStringLiteral("CA")},
    {QStringLiteral("America/St_Johns"),        QStringLiteral("CA")},
    {QStringLiteral("America/Mexico_City"),     QStringLiteral("MX")},
    {QStringLiteral("America/Sao_Paulo"),       QStringLiteral("BR")},
    {QStringLiteral("America/Buenos_Aires"),    QStringLiteral("AR")},
    {QStringLiteral("America/Santiago"),        QStringLiteral("CL")},
    {QStringLiteral("America/Lima"),            QStringLiteral("PE")},
    {QStringLiteral("America/Bogota"),          QStringLiteral("CO")},
    {QStringLiteral("America/Caracas"),         QStringLiteral("VE")},
    // Europe
    {QStringLiteral("Europe/London"),           QStringLiteral("GB")},
    {QStringLiteral("Europe/Paris"),            QStringLiteral("FR")},
    {QStringLiteral("Europe/Berlin"),           QStringLiteral("DE")},
    {QStringLiteral("Europe/Amsterdam"),        QStringLiteral("NL")},
    {QStringLiteral("Europe/Brussels"),         QStringLiteral("BE")},
    {QStringLiteral("Europe/Madrid"),           QStringLiteral("ES")},
    {QStringLiteral("Europe/Rome"),             QStringLiteral("IT")},
    {QStringLiteral("Europe/Lisbon"),           QStringLiteral("PT")},
    {QStringLiteral("Europe/Zurich"),           QStringLiteral("CH")},
    {QStringLiteral("Europe/Vienna"),           QStringLiteral("AT")},
    {QStringLiteral("Europe/Warsaw"),           QStringLiteral("PL")},
    {QStringLiteral("Europe/Prague"),           QStringLiteral("CZ")},
    {QStringLiteral("Europe/Budapest"),         QStringLiteral("HU")},
    {QStringLiteral("Europe/Bucharest"),        QStringLiteral("RO")},
    {QStringLiteral("Europe/Sofia"),            QStringLiteral("BG")},
    {QStringLiteral("Europe/Stockholm"),        QStringLiteral("SE")},
    {QStringLiteral("Europe/Oslo"),             QStringLiteral("NO")},
    {QStringLiteral("Europe/Copenhagen"),       QStringLiteral("DK")},
    {QStringLiteral("Europe/Helsinki"),         QStringLiteral("FI")},
    {QStringLiteral("Europe/Athens"),           QStringLiteral("GR")},
    {QStringLiteral("Europe/Kiev"),             QStringLiteral("UA")},
    {QStringLiteral("Europe/Kyiv"),             QStringLiteral("UA")},
    {QStringLiteral("Europe/Moscow"),           QStringLiteral("RU")},
    {QStringLiteral("Europe/Istanbul"),         QStringLiteral("TR")},
    {QStringLiteral("Europe/Riga"),             QStringLiteral("LV")},
    {QStringLiteral("Europe/Vilnius"),          QStringLiteral("LT")},
    {QStringLiteral("Europe/Tallinn"),          QStringLiteral("EE")},
    {QStringLiteral("Europe/Dublin"),           QStringLiteral("IE")},
    {QStringLiteral("Europe/Bratislava"),       QStringLiteral("SK")},
    {QStringLiteral("Europe/Ljubljana"),        QStringLiteral("SI")},
    {QStringLiteral("Europe/Zagreb"),           QStringLiteral("HR")},
    {QStringLiteral("Europe/Sarajevo"),         QStringLiteral("BA")},
    {QStringLiteral("Europe/Belgrade"),         QStringLiteral("RS")},
    {QStringLiteral("Europe/Skopje"),           QStringLiteral("MK")},
    {QStringLiteral("Europe/Podgorica"),        QStringLiteral("ME")},
    {QStringLiteral("Europe/Tirane"),           QStringLiteral("AL")},
    {QStringLiteral("Europe/Minsk"),            QStringLiteral("BY")},
    {QStringLiteral("Europe/Luxembourg"),       QStringLiteral("LU")},
    {QStringLiteral("Europe/Malta"),            QStringLiteral("MT")},
    {QStringLiteral("Europe/Nicosia"),          QStringLiteral("CY")},
    {QStringLiteral("Atlantic/Reykjavik"),      QStringLiteral("IS")},
    // Asia
    {QStringLiteral("Asia/Tokyo"),              QStringLiteral("JP")},
    {QStringLiteral("Asia/Seoul"),              QStringLiteral("KR")},
    {QStringLiteral("Asia/Shanghai"),           QStringLiteral("CN")},
    {QStringLiteral("Asia/Hong_Kong"),          QStringLiteral("HK")},
    {QStringLiteral("Asia/Singapore"),          QStringLiteral("SG")},
    {QStringLiteral("Asia/Bangkok"),            QStringLiteral("TH")},
    {QStringLiteral("Asia/Jakarta"),            QStringLiteral("ID")},
    {QStringLiteral("Asia/Manila"),             QStringLiteral("PH")},
    {QStringLiteral("Asia/Kuala_Lumpur"),       QStringLiteral("MY")},
    {QStringLiteral("Asia/Kolkata"),            QStringLiteral("IN")},
    {QStringLiteral("Asia/Karachi"),            QStringLiteral("PK")},
    {QStringLiteral("Asia/Dhaka"),              QStringLiteral("BD")},
    {QStringLiteral("Asia/Colombo"),            QStringLiteral("LK")},
    {QStringLiteral("Asia/Kathmandu"),          QStringLiteral("NP")},
    {QStringLiteral("Asia/Tashkent"),           QStringLiteral("UZ")},
    {QStringLiteral("Asia/Almaty"),             QStringLiteral("KZ")},
    {QStringLiteral("Asia/Tehran"),             QStringLiteral("IR")},
    {QStringLiteral("Asia/Baghdad"),            QStringLiteral("IQ")},
    {QStringLiteral("Asia/Riyadh"),             QStringLiteral("SA")},
    {QStringLiteral("Asia/Dubai"),              QStringLiteral("AE")},
    {QStringLiteral("Asia/Kuwait"),             QStringLiteral("KW")},
    {QStringLiteral("Asia/Qatar"),              QStringLiteral("QA")},
    {QStringLiteral("Asia/Beirut"),             QStringLiteral("LB")},
    {QStringLiteral("Asia/Damascus"),           QStringLiteral("SY")},
    {QStringLiteral("Asia/Amman"),              QStringLiteral("JO")},
    {QStringLiteral("Asia/Jerusalem"),          QStringLiteral("IL")},
    {QStringLiteral("Asia/Nicosia"),            QStringLiteral("CY")},
    {QStringLiteral("Asia/Taipei"),             QStringLiteral("TW")},
    {QStringLiteral("Asia/Ulaanbaatar"),        QStringLiteral("MN")},
    {QStringLiteral("Asia/Yerevan"),            QStringLiteral("AM")},
    {QStringLiteral("Asia/Tbilisi"),            QStringLiteral("GE")},
    {QStringLiteral("Asia/Baku"),               QStringLiteral("AZ")},
    // Oceania
    {QStringLiteral("Australia/Sydney"),        QStringLiteral("AU")},
    {QStringLiteral("Australia/Melbourne"),     QStringLiteral("AU")},
    {QStringLiteral("Australia/Brisbane"),      QStringLiteral("AU")},
    {QStringLiteral("Australia/Perth"),         QStringLiteral("AU")},
    {QStringLiteral("Australia/Adelaide"),      QStringLiteral("AU")},
    {QStringLiteral("Pacific/Auckland"),        QStringLiteral("NZ")},
    // Africa
    {QStringLiteral("Africa/Cairo"),            QStringLiteral("EG")},
    {QStringLiteral("Africa/Lagos"),            QStringLiteral("NG")},
    {QStringLiteral("Africa/Nairobi"),          QStringLiteral("KE")},
    {QStringLiteral("Africa/Johannesburg"),     QStringLiteral("ZA")},
    {QStringLiteral("Africa/Casablanca"),       QStringLiteral("MA")},
    {QStringLiteral("Africa/Algiers"),          QStringLiteral("DZ")},
    {QStringLiteral("Africa/Tunis"),            QStringLiteral("TN")},
    {QStringLiteral("Africa/Tripoli"),          QStringLiteral("LY")},
    {QStringLiteral("Africa/Accra"),            QStringLiteral("GH")},
    {QStringLiteral("Africa/Addis_Ababa"),      QStringLiteral("ET")},
};

// ---------------------------------------------------------------------------
// detectUserCountry — try timezone first, then locale territory
// ---------------------------------------------------------------------------
QString CountriesPage::detectUserCountry()
{
    // 1. Try system timezone
    const QByteArray tzId = QTimeZone::systemTimeZoneId();
    if (!tzId.isEmpty())
    {
        const QString tzStr = QString::fromUtf8(tzId);
        const auto it = kTimezoneToCountry.find(tzStr);
        if (it != kTimezoneToCountry.end())
            return it.value();
    }

    // 2. Try QLocale territory
    const QLocale sysLocale = QLocale::system();
    const QLocale::Territory territory = sysLocale.territory();
    if (territory != QLocale::AnyTerritory)
    {
        // QLocale::territoryToCode() exists in Qt 6.2+; fall back to
        // bcp47Name parsing (e.g. "en-US" → "US") for wider compatibility.
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        const QString code = QLocale::territoryToCode(territory);
        if (!code.isEmpty())
            return code.toUpper();
#endif
        // Fallback: parse the BCP-47 name
        const QString bcp47 = sysLocale.bcp47Name(); // e.g. "en-US", "de-DE"
        const qsizetype dashPos = bcp47.indexOf(QLatin1Char('-'));
        if (dashPos != -1)
        {
            const QString regionTag = bcp47.mid(dashPos + 1).toUpper();
            // Only accept if it looks like a 2-letter ISO country code
            if (regionTag.length() == 2 && regionTag[0].isLetter() && regionTag[1].isLetter())
                return regionTag;
        }
    }

    return {}; // detection failed
}

// ---------------------------------------------------------------------------
// svgPixmap — render an SVG resource file into a QPixmap at requested size
// ---------------------------------------------------------------------------
QPixmap CountriesPage::svgPixmap(const QString& resourcePath, int size)
{
    QSvgRenderer renderer(resourcePath);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return pixmap;
}

CountriesPage::CountriesPage(VpnManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_localCountryCode(detectUserCountry())
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // Header row
    auto* headerRow = new QHBoxLayout();
    auto* titleLabel = new QLabel(QStringLiteral("Select Server"), this);
    titleLabel->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(titleLabel);
    headerRow->addStretch();
    m_refreshBtn = new QPushButton(QStringLiteral("↻ Refresh"), this);
    m_refreshBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_refreshBtn->setFixedHeight(30);
    connect(m_refreshBtn, &QPushButton::clicked, this, &CountriesPage::refresh);
    headerRow->addWidget(m_refreshBtn);
    layout->addLayout(headerRow);

    // Search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("inputField"));
    m_searchEdit->setPlaceholderText(QStringLiteral("Search countries…"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CountriesPage::filterCountries);
    layout->addWidget(m_searchEdit);

    // Splitter for countries / cities
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Countries list
    auto* countriesWidget = new QWidget(splitter);
    auto* countriesLayout = new QVBoxLayout(countriesWidget);
    countriesLayout->setContentsMargins(0, 0, 0, 0);
    auto* countriesLabel = new QLabel(QStringLiteral("Countries"), countriesWidget);
    countriesLabel->setObjectName(QStringLiteral("listHeader"));
    countriesLayout->addWidget(countriesLabel);
    m_countriesList = new QListWidget(countriesWidget);
    m_countriesList->setObjectName(QStringLiteral("serverList"));
    connect(m_countriesList, &QListWidget::itemClicked, this, &CountriesPage::onCountrySelected);
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
    connect(m_citiesList, &QListWidget::itemClicked, this, &CountriesPage::onCitySelected);
    citiesLayout->addWidget(m_citiesList);
    splitter->addWidget(citiesWidget);

    splitter->setSizes({200, 200});
    layout->addWidget(splitter);

    // Connect button
    m_connectBtn = new QPushButton(QStringLiteral("Connect to Selected"), this);
    m_connectBtn->setObjectName(QStringLiteral("primaryButton"));
    m_connectBtn->setEnabled(false);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    connect(m_connectBtn, &QPushButton::clicked, this, [this]()
    {
        emit connectRequested(m_selectedCode, m_selectedCity);
    });
    layout->addWidget(m_connectBtn);

    // Spinner timer — updates the loading label every 200 ms
    static constexpr const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static constexpr int frameCount = 10;
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_spinnerFrame = (m_spinnerFrame + 1) % frameCount;
        if (m_citiesList->count() > 0)
            m_citiesList->item(0)->setText(
                QStringLiteral("%1 Loading cities…").arg(QString::fromUtf8(frames[m_spinnerFrame])));
    });

    m_countriesSpinnerTimer = new QTimer(this);
    m_countriesSpinnerTimer->setInterval(200);
    connect(m_countriesSpinnerTimer, &QTimer::timeout, this, [this]()
    {
        m_countriesSpinnerFrame = (m_countriesSpinnerFrame + 1) % frameCount;
        if (m_countriesList->count() > 0)
            m_countriesList->item(0)->setText(
                QStringLiteral("%1 Loading countries…").arg(QString::fromUtf8(frames[m_countriesSpinnerFrame])));
    });

    // Wire VpnManager signals
    connect(m_manager, &VpnManager::countriesReady, this, &CountriesPage::onCountriesReady);
    connect(m_manager, &VpnManager::citiesReady, this,
            [this](const QString& code, const QList<QPair<QString, QString>>& cities)
            { onCitiesReady(code, cities); });
}

void CountriesPage::refresh()
{
    m_countriesList->clear();
    m_citiesList->clear();
    m_selectedCountry.clear();
    m_selectedCode.clear();
    m_selectedCity.clear();
    m_connectBtn->setEnabled(false);
    m_refreshBtn->setEnabled(false);
    m_refreshBtn->setText(QStringLiteral("Loading…"));

    m_countriesSpinnerFrame = 0;
    auto* loadingItem = new QListWidgetItem(QStringLiteral("⠋ Loading countries…"));
    loadingItem->setFlags(Qt::NoItemFlags);
    loadingItem->setForeground(QColor(0x99, 0x99, 0xbb));
    m_countriesList->addItem(loadingItem);
    m_countriesSpinnerTimer->start();

    m_manager->fetchCountries();
}

void CountriesPage::onCountriesReady(const QMap<QString, QString>& countries)
{
    m_countriesSpinnerTimer->stop();
    m_allCountries = countries;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(QStringLiteral("↻ Refresh"));
    filterCountries(m_searchEdit->text());
}

void CountriesPage::onCitiesReady(const QString& countryCode,
                                  const QList<QPair<QString, QString>>& cities)
{
    // Stop spinner
    m_spinnerTimer->stop();
    m_citiesList->clear();

    const QString displayName = m_allCountries.key(countryCode, countryCode);
    m_citiesLabel->setText(QStringLiteral("Cities – %1").arg(displayName));

    // "⚡  Fastest server" option always first - styled to stand out from plain city rows
    auto* anyItem = new QListWidgetItem(QStringLiteral("⚡  Fastest server"));
    anyItem->setData(Qt::UserRole, QString());
    anyItem->setToolTip(QStringLiteral("Connects to the fastest available server in this country."));
    // Accent purple text, bold-italic, subtle tinted background
    anyItem->setForeground(QColor(0xab, 0x8f, 0xff));
    QFont fastestFont = m_citiesList->font();
    fastestFont.setBold(true);
    fastestFont.setItalic(true);
    anyItem->setFont(fastestFont);
    anyItem->setBackground(QColor(0x6d, 0x4a, 0xff, 40));
    m_citiesList->addItem(anyItem);

    for (const auto& [city, features] : cities)
        addCityItem(city, features);
}

// ---------------------------------------------------------------------------
// addCityItem — adds a city row with inline SVG feature icons + tooltips
// ---------------------------------------------------------------------------
void CountriesPage::addCityItem(const QString& city, const QString& features)
{
    // Parse feature tags (comma-separated, e.g. "P2P, Secure Core, Tor")
    const QStringList tags = features.split(QLatin1Char(','), Qt::SkipEmptyParts);

    // When setItemWidget() is used Qt gives the widget the item's inner rect
    // after the stylesheet's "padding: 8px 12px" is already applied, so we
    // must NOT add our own margins — doing so would double-pad and clip text.
    // We also set an explicit sizeHint that matches the plain country items:
    //   font height (~15px) + top padding (8) + bottom padding (8) + border (1) = ~32px
    auto* row = new QWidget();
    row->setAttribute(Qt::WA_TranslucentBackground);
    auto* hbox = new QHBoxLayout(row);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(6);
    hbox->setAlignment(Qt::AlignVCenter);

    auto* cityLabel = new QLabel(city, row);
    cityLabel->setObjectName(QStringLiteral("cityLabel"));
    cityLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    hbox->addWidget(cityLabel, 0, Qt::AlignVCenter);
    hbox->addStretch();

    // Feature icon metadata: keyword → {resource path, tooltip text}
    struct FeatureMeta { QString keyword; QString resource; QString tooltip; };
    static const FeatureMeta kFeatures[] = {
        { QStringLiteral("p2p"),         QStringLiteral(":/assets/server-p2p.svg"),
          QStringLiteral("P2P — Optimized for peer-to-peer file sharing") },
        { QStringLiteral("secure core"), QStringLiteral(":/assets/server-secure-core.svg"),
          QStringLiteral("Secure Core — Routes traffic through privacy-friendly countries for extra protection") },
        { QStringLiteral("tor"),         QStringLiteral(":/assets/server-tor.svg"),
          QStringLiteral("Tor — Routes traffic through the Tor anonymity network") },
    };

    for (const auto& meta : kFeatures)
    {
        // Check if any tag matches this feature (case-insensitive substring)
        bool matched = false;
        for (const QString& tag : tags)
        {
            if (tag.trimmed().contains(meta.keyword, Qt::CaseInsensitive))
            {
                matched = true;
                break;
            }
        }
        if (!matched)
            continue;

        auto* iconLabel = new QLabel(row);
        iconLabel->setPixmap(svgPixmap(meta.resource, 16));
        iconLabel->setFixedSize(24, 24);
        iconLabel->setScaledContents(false);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setToolTip(meta.tooltip);
        hbox->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    row->setLayout(hbox);

    // Use the countries list's own row height so both panels are identical.
    // sizeHintForRow() asks Qt's style engine for the true measured height
    // (font + stylesheet padding + border) without any hard-coded values.
    // We use row 0 if available; if the list is somehow empty we fall back to
    // the cities list's own row height, and finally to a font-based estimate.
    int itemH = -1;
    if (m_countriesList->count() > 0)
        itemH = m_countriesList->sizeHintForRow(0);
    if (itemH <= 0 && m_citiesList->count() > 0)
        itemH = m_citiesList->sizeHintForRow(0);
    if (itemH <= 0)
        itemH = m_countriesList->fontMetrics().height() + 17;

    auto* item = new QListWidgetItem();
    item->setData(Qt::UserRole, city);
    item->setSizeHint(QSize(0, itemH));
    m_citiesList->addItem(item);
    m_citiesList->setItemWidget(item, row);
}

void CountriesPage::onCountrySelected(QListWidgetItem* item)
{
    // Strip the pinned-country star prefix if present
    QString displayName = item->text();
    if (displayName.startsWith(QStringLiteral("★ ")))
        displayName = displayName.mid(2);

    m_selectedCountry = displayName;
    m_selectedCode = item->data(Qt::UserRole).toString();
    m_selectedCity.clear();
    m_citiesList->clear();
    m_citiesLabel->setText(QStringLiteral("Cities – %1").arg(m_selectedCountry));
    m_connectBtn->setEnabled(!m_selectedCode.isEmpty());
    m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));

    if (!m_selectedCode.isEmpty())
    {
        // Insert a non-selectable spinner item while waiting for cities
        m_spinnerFrame = 0;
        auto* loadingItem = new QListWidgetItem(QStringLiteral("⠋ Loading cities…"));
        loadingItem->setFlags(Qt::NoItemFlags);
        loadingItem->setForeground(QColor(0x99, 0x99, 0xbb));
        m_citiesList->addItem(loadingItem);
        m_spinnerTimer->start();
        m_manager->fetchCities(m_selectedCode);
    }
}

void CountriesPage::onCitySelected(QListWidgetItem* item)
{
    m_selectedCity = item->data(Qt::UserRole).toString();
    if (m_selectedCity.isEmpty())
    {
        m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));
    }
    else
    {
        m_connectBtn->setText(QStringLiteral("Connect to %1, %2").arg(m_selectedCountry, m_selectedCity));
    }
}

void CountriesPage::filterCountries(const QString& text) const
{
    m_countriesList->clear();

    // --- Collect matching entries ---
    // We split them into two buckets: local country (pinned) and the rest.
    QListWidgetItem* pinnedItem = nullptr;
    QList<QListWidgetItem*> otherItems;

    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it)
    {
        const QString& name = it.key();
        const QString& code = it.value();
        if (!text.isEmpty() && !name.contains(text, Qt::CaseInsensitive))
            continue;

        auto* item = new QListWidgetItem(name);
        item->setData(Qt::UserRole, code);

        // Pin the user's detected country
        if (!m_localCountryCode.isEmpty() &&
            code.compare(m_localCountryCode, Qt::CaseInsensitive) == 0)
        {
            // Mark it visually so the user knows it was auto-detected
            item->setText(QStringLiteral("★ %1").arg(name));
            pinnedItem = item;
        }
        else
        {
            otherItems.append(item);
        }
    }

    // Insert pinned entry first (if it matched the filter), then the rest
    if (pinnedItem)
        m_countriesList->addItem(pinnedItem);
    for (auto* item : otherItems)
        m_countriesList->addItem(item);
}
