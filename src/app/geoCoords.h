#pragma once
// geoCoords.h
// GeoCoords - QML-facing lookup of approximate country centroid coordinates
// (latitude / longitude). Used by the GlobeView connection animation to plot
// the origin (user country) and the destination (connected country).
//
// Registered as a QML singleton: import Vela; GeoCoords.coordsFor("US").
// Coordinates are approximate country centroids - good enough for a visual,
// not for navigation. Registered as a QML singleton.

#include <QObject>
#include <QQmlEngine>
#include <QPointF>
#include <QString>

class GeoCoords : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static GeoCoords* instance();
    static GeoCoords* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    // Returns {lat, lon} for a 2-letter uppercase ISO 3166-1 alpha-2 code,
    // or an invalid QPointF when unknown.
    Q_INVOKABLE QPointF coordsFor(const QString& countryCode) const;

    // Best-effort user country code (uppercase) via system timezone / locale.
    Q_INVOKABLE QString userCountry() const;

private:
    explicit GeoCoords(QObject* parent = nullptr);
};