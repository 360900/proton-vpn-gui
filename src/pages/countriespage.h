#pragma once

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include "../vpnmanager.h"

// ---------------------------------------------------------------------------
// CountriesPage – server browser with feature-filter bubbles and a
// responsive layout (wide = side-by-side splitter, narrow = accordion list).
// ---------------------------------------------------------------------------
class CountriesPage : public QWidget
{
    Q_OBJECT

public:
    explicit CountriesPage(VpnManager* manager, QWidget* parent = nullptr);
    void refresh();

signals:
    void connectRequested(const QString& countryCode, const QString& city);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    VpnManager* m_manager;
    QString     m_localCountryCode;

    QMap<QString, QString> m_allCountries; // name → code
    QMap<QString, QList<QPair<QString, QString>>> m_cityCache; // code → cities
    QSet<QString> m_pendingCityCodes;

    QString m_selectedCode;
    QString m_selectedCity;
    QString m_selectedCountry;

    static constexpr int kNarrowThreshold = 560;
    bool m_narrowMode = false;

    bool m_filterP2P        = false;
    bool m_filterSecureCore = false;
    bool m_filterTor        = false;

    QLineEdit*   m_searchEdit     = nullptr;
    QPushButton* m_refreshBtn     = nullptr;
    QLabel*      m_countriesLabel = nullptr;

    QPushButton* m_bubbleAll        = nullptr;
    QPushButton* m_bubbleP2P        = nullptr;
    QPushButton* m_bubbleSecureCore = nullptr;
    QPushButton* m_bubbleTor        = nullptr;

    // Wide layout
    QWidget*     m_wideWidget    = nullptr;
    QListWidget* m_countriesList = nullptr;
    QListWidget* m_citiesList    = nullptr;
    QLabel*      m_citiesLabel   = nullptr;
    QPushButton* m_connectBtn    = nullptr;

    // Narrow layout
    QWidget*     m_narrowWidget  = nullptr;
    QScrollArea* m_narrowScroll  = nullptr;
    QWidget*     m_narrowContent = nullptr;
    QVBoxLayout* m_narrowLayout  = nullptr;

    struct AccordionItem {
        QToolButton* headerBtn    = nullptr;
        QWidget*     citiesWidget = nullptr;
        QVBoxLayout* citiesLayout = nullptr;
        bool         expanded     = false;
    };
    QMap<QString, AccordionItem> m_accordion;

    QTimer* m_countriesSpinnerTimer = nullptr;
    QTimer* m_spinnerTimer          = nullptr;
    int m_countriesSpinnerFrame = 0;
    int m_spinnerFrame          = 0;

    void buildWideLayout(QVBoxLayout* parent);
    void buildNarrowLayout(QVBoxLayout* parent);
    void switchLayout(bool narrow);

    void applyFilter();
    void updateBubbleStyles();
    bool countryPassesFilter(const QString& code) const;

    void populateWide();
    void populateNarrow();

    void addWideCityItem(const QString& city, const QString& features);
    void addNarrowCityItem(QVBoxLayout* layout, const QString& city,
                           const QString& features, const QString& code);

    void onCountriesReady(const QMap<QString, QString>& countries);
    void onCitiesReady(const QString& code, const QList<QPair<QString, QString>>& cities);

    void onWideCountrySelected(QListWidgetItem* item);
    void onWideCitySelected(QListWidgetItem* item);

    void toggleAccordion(const QString& code);
    void ensureCities(const QString& code);

    static QString bubbleStyle(bool active);
};
