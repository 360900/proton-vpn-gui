#include <QtTest/QtTest>
#include "geoutils.h"

class TstGeoUtils : public QObject
{
    Q_OBJECT

private slots:
    // countryCodeToName -------------------------------------------------------
    void countryCodeToName_knownCode_returnsName()
    {
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("US")), QStringLiteral("United States"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("DE")), QStringLiteral("Germany"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("CH")), QStringLiteral("Switzerland"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("JP")), QStringLiteral("Japan"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("GB")), QStringLiteral("United Kingdom"));
    }

    void countryCodeToName_lowercaseInput_returnsName()
    {
        // Input is normalised to upper-case internally.
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("us")), QStringLiteral("United States"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("fr")), QStringLiteral("France"));
    }

    void countryCodeToName_unknownCode_returnsCodeItself()
    {
        // The function documents that it returns the original code when unknown.
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("XX")), QStringLiteral("XX"));
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("ZZ")), QStringLiteral("ZZ"));
    }

    void countryCodeToName_emptyString_returnsEmptyString()
    {
        QCOMPARE(GeoUtils::countryCodeToName(QString()), QString());
    }

    void countryCodeToName_ukAlias_returnsSameAsGb()
    {
        // Both "GB" and "UK" are mapped to "United Kingdom".
        QCOMPARE(GeoUtils::countryCodeToName(QStringLiteral("UK")),
                 GeoUtils::countryCodeToName(QStringLiteral("GB")));
    }
};

QTEST_MAIN(TstGeoUtils)
#include "tst_geoutils.moc"

