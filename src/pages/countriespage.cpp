#include "countriespage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPair>

CountriesPage::CountriesPage(VpnManager *manager, QWidget *parent)
    : QWidget(parent), m_manager(manager)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // Header row
    auto *headerRow = new QHBoxLayout();
    auto *titleLabel = new QLabel(QStringLiteral("Select Server"), this);
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
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Countries list
    auto *countriesWidget = new QWidget(splitter);
    auto *countriesLayout = new QVBoxLayout(countriesWidget);
    countriesLayout->setContentsMargins(0, 0, 0, 0);
    auto *countriesLabel = new QLabel(QStringLiteral("Countries"), countriesWidget);
    countriesLabel->setObjectName(QStringLiteral("listHeader"));
    countriesLayout->addWidget(countriesLabel);
    m_countriesList = new QListWidget(countriesWidget);
    m_countriesList->setObjectName(QStringLiteral("serverList"));
    connect(m_countriesList, &QListWidget::itemClicked, this, &CountriesPage::onCountrySelected);
    countriesLayout->addWidget(m_countriesList);
    splitter->addWidget(countriesWidget);

    // Cities panel
    auto *citiesWidget = new QWidget(splitter);
    auto *citiesLayout = new QVBoxLayout(citiesWidget);
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
    connect(m_connectBtn, &QPushButton::clicked, this, [this]() {
        emit connectRequested(m_selectedCode, m_selectedCity);
    });
    layout->addWidget(m_connectBtn);

    // Spinner timer — updates the loading label every 200 ms
    static const char *const frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    static constexpr int frameCount = 10;
    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(200);
    connect(m_spinnerTimer, &QTimer::timeout, this, [this, frames]() {
        m_spinnerFrame = (m_spinnerFrame + 1) % frameCount;
        if (m_citiesList->count() > 0)
            m_citiesList->item(0)->setText(
                QStringLiteral("%1 Loading cities…").arg(QString::fromUtf8(frames[m_spinnerFrame])));
    });

    m_countriesSpinnerTimer = new QTimer(this);
    m_countriesSpinnerTimer->setInterval(200);
    connect(m_countriesSpinnerTimer, &QTimer::timeout, this, [this, frames]() {
        m_countriesSpinnerFrame = (m_countriesSpinnerFrame + 1) % frameCount;
        if (m_countriesList->count() > 0)
            m_countriesList->item(0)->setText(
                QStringLiteral("%1 Loading countries…").arg(QString::fromUtf8(frames[m_countriesSpinnerFrame])));
    });

    // Wire VpnManager signals
    connect(m_manager, &VpnManager::countriesReady, this, &CountriesPage::onCountriesReady);
    connect(m_manager, &VpnManager::citiesReady, this, &CountriesPage::onCitiesReady);
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
    auto *loadingItem = new QListWidgetItem(QStringLiteral("⠋ Loading countries…"));
    loadingItem->setFlags(Qt::NoItemFlags);
    loadingItem->setForeground(QColor(0x99, 0x99, 0xbb));
    m_countriesList->addItem(loadingItem);
    m_countriesSpinnerTimer->start();

    m_manager->fetchCountries();
}

void CountriesPage::onCountriesReady(const QMap<QString, QString> &countries)
{
    m_countriesSpinnerTimer->stop();
    m_allCountries = countries;
    m_refreshBtn->setEnabled(true);
    m_refreshBtn->setText(QStringLiteral("↻ Refresh"));
    filterCountries(m_searchEdit->text());
}

void CountriesPage::onCitiesReady(const QString &countryCode,
                                   const QList<QPair<QString, QString>> &cities)
{
    // Stop spinner
    m_spinnerTimer->stop();
    m_citiesList->clear();

    const QString displayName = m_allCountries.key(countryCode, countryCode);
    m_citiesLabel->setText(QStringLiteral("Cities – %1").arg(displayName));

    // "Any city" option always first
    auto *anyItem = new QListWidgetItem(QStringLiteral("Any city"));
    anyItem->setData(Qt::UserRole, QString());
    m_citiesList->addItem(anyItem);

    for (const auto &[city, features] : cities) {
        const QString label = features.isEmpty()
            ? city
            : QStringLiteral("%1  ·  %2").arg(city, features);
        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, city);
        m_citiesList->addItem(item);
    }
}

void CountriesPage::onCountrySelected(QListWidgetItem *item)
{
    m_selectedCountry = item->text();
    m_selectedCode    = item->data(Qt::UserRole).toString();
    m_selectedCity.clear();
    m_citiesList->clear();
    m_citiesLabel->setText(QStringLiteral("Cities – %1").arg(m_selectedCountry));
    m_connectBtn->setEnabled(!m_selectedCode.isEmpty());
    m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));

    if (!m_selectedCode.isEmpty()) {
        // Insert a non-selectable spinner item while waiting for cities
        m_spinnerFrame = 0;
        auto *loadingItem = new QListWidgetItem(QStringLiteral("⠋ Loading cities…"));
        loadingItem->setFlags(Qt::NoItemFlags);
        loadingItem->setForeground(QColor(0x99, 0x99, 0xbb));
        m_citiesList->addItem(loadingItem);
        m_spinnerTimer->start();
        m_manager->fetchCities(m_selectedCode);
    }
}

void CountriesPage::onCitySelected(QListWidgetItem *item)
{
    m_selectedCity = item->data(Qt::UserRole).toString();
    if (m_selectedCity.isEmpty()) {
        m_connectBtn->setText(QStringLiteral("Connect to %1").arg(m_selectedCountry));
    } else {
        m_connectBtn->setText(QStringLiteral("Connect to %1, %2").arg(m_selectedCountry, m_selectedCity));
    }
}

void CountriesPage::filterCountries(const QString &text)
{
    m_countriesList->clear();
    for (auto it = m_allCountries.constBegin(); it != m_allCountries.constEnd(); ++it) {
        const QString &name = it.key();
        const QString &code = it.value();
        if (text.isEmpty() || name.contains(text, Qt::CaseInsensitive)) {
            auto *item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, code);
            m_countriesList->addItem(item);
        }
    }
}
