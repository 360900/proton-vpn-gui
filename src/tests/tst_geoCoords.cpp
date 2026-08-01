#include <QtTest/QtTest>
#include "../app/geoCoords.h"

class TstGeoCoords : public QObject
{
    Q_OBJECT

private slots:
    void coordsFor_knownCode_returnsValidPoint()
    {
        const QPointF us = GeoCoords::instance()->coordsFor(QStringLiteral("US"));
        QVERIFY(!us.isNull());
        QVERIFY(us.x() >= -90 && us.x() <= 90);   // latitude range
        QVERIFY(us.y() >= -180 && us.y() <= 180);  // longitude range
    }

    void coordsFor_caseInsensitive_returnsValid()
    {
        QCOMPARE(GeoCoords::instance()->coordsFor(QStringLiteral("de")),
                 GeoCoords::instance()->coordsFor(QStringLiteral("DE")));
    }

    void coordsFor_unknownCode_returnsInvalid()
    {
        QVERIFY(GeoCoords::instance()->coordsFor(QStringLiteral("ZZ")).isNull());
    }

    void coordsFor_wrongLength_returnsInvalid()
    {
        QVERIFY(GeoCoords::instance()->coordsFor(QStringLiteral("USA")).isNull());
        QVERIFY(GeoCoords::instance()->coordsFor(QString()).isNull());
    }

    void coordsFor_usIsRoughlyCentral()
    {
        // The US centroid lives in the lower-48 longitude band.
        const QPointF us = GeoCoords::instance()->coordsFor(QStringLiteral("US"));
        QVERIFY(us.x() > 20 && us.x() < 60);
        QVERIFY(us.y() < -60 && us.y() > -130);
    }
};

QTEST_MAIN(TstGeoCoords)
#include "tst_geoCoords.moc"