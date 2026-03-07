#pragma once

#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QTimer>
#include <QSvgRenderer>
#include "../vpnmanager.h"

class CountriesPage : public QWidget
{
    Q_OBJECT

public:
    explicit CountriesPage(VpnManager* manager, QWidget* parent = nullptr);

    void refresh();

signals:
    void connectRequested(const QString& country, const QString& city);

private:
    VpnManager* m_manager;
    QLineEdit* m_searchEdit;
    QListWidget* m_countriesList;
    QListWidget* m_citiesList;
    QLabel* m_citiesLabel;
    QPushButton* m_connectBtn;
    QPushButton* m_refreshBtn;
    QTimer* m_spinnerTimer;
    QTimer* m_countriesSpinnerTimer;
    int m_spinnerFrame = 0;
    int m_countriesSpinnerFrame = 0;

    QString m_selectedCountry; // display name
    QString m_selectedCode;    // 2-letter code passed to CLI
    QString m_selectedCity;
    QMap<QString, QString> m_allCountries; // name → code
    QString m_localCountryCode;            // detected local country (may be empty)

    // Detect the user's country via timezone then locale fallback.
    // Returns a 2-letter uppercase country code, or empty string on failure.
    static QString detectUserCountry();

    // Build a small icon+tooltip widget for a set of feature tags and add it
    // to the cities list as a custom item widget.
    void addCityItem(const QString& city, const QString& features);

    // Render an SVG resource to a QPixmap at the given size.
    static QPixmap svgPixmap(const QString& resourcePath, int size = 16);

    void onCountriesReady(const QMap<QString, QString>& countries);
    void onCitiesReady(const QString& countryCode, const QList<QPair<QString, QString>>& cities);
    void onCountrySelected(QListWidgetItem* item);
    void onCitySelected(QListWidgetItem* item);
    void filterCountries(const QString& text) const;
};
