#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QTimer>
#include "../vpnmanager.h"

class CountriesPage : public QWidget
{
    Q_OBJECT
public:
    explicit CountriesPage(VpnManager *manager, QWidget *parent = nullptr);

    void refresh();

signals:
    void connectRequested(const QString &country, const QString &city);

private:
    VpnManager *m_manager;
    QLineEdit *m_searchEdit;
    QListWidget *m_countriesList;
    QListWidget *m_citiesList;
    QLabel *m_citiesLabel;
    QPushButton *m_connectBtn;
    QPushButton *m_refreshBtn;
    QTimer *m_spinnerTimer;
    QTimer *m_countriesSpinnerTimer;
    int m_spinnerFrame = 0;
    int m_countriesSpinnerFrame = 0;

    QString m_selectedCountry;  // display name
    QString m_selectedCode;     // 2-letter code passed to CLI
    QString m_selectedCity;
    QMap<QString, QString> m_allCountries;  // name → code

    void onCountriesReady(const QMap<QString, QString> &countries);
    void onCitiesReady(const QString &countryCode, const QList<QPair<QString, QString>> &cities);
    void onCountrySelected(QListWidgetItem *item);
    void onCitySelected(QListWidgetItem *item);
    void filterCountries(const QString &text);
};
