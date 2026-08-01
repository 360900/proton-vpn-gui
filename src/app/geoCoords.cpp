// geoCoords.cpp
// See geoCoords.h.

#include "geoCoords.h"

#include "../geoUtils.h"

#include <QHash>
#include <QString>

namespace
{
struct Centroid
{
    const char* code;
    double lat;
    double lon;
};

// Approximate country centroids (lat, lon). Covers the countries ProtonVPN
// commonly offers plus a few extras; unknown codes simply return invalid.
constexpr Centroid CENTROIDS[] = {
    {"AF", 33.9, 67.7}, {"AL", 41.2, 20.2}, {"AR", -38.4, -63.6}, {"AT", 47.2, 14.5},
    {"AU", -25.3, 133.8}, {"AZ", 40.1, 47.6}, {"BA", 44.3, 17.7}, {"BD", 23.7, 90.4},
    {"BE", 50.5, 4.5}, {"BG", 42.7, 25.2}, {"BR", -14.2, -51.9}, {"BS", 25.0, -77.4},
    {"BY", 53.7, 27.9}, {"CA", 56.1, -106.3}, {"CH", 46.8, 8.2}, {"CL", -35.7, -71.5},
    {"CN", 35.9, 104.2}, {"CO", 4.6, -74.3}, {"CR", 9.7, -83.8}, {"HR", 45.1, 15.2},
    {"CY", 35.1, 33.4}, {"CZ", 49.8, 15.5}, {"DE", 51.2, 10.4}, {"DK", 56.3, 9.5},
    {"DO", 18.7, -70.7}, {"EC", -1.8, -78.2}, {"EE", 58.6, 25.0}, {"EG", 26.8, 30.8},
    {"ES", 40.5, -3.7}, {"FI", 61.9, 25.7}, {"FR", 46.2, 2.2}, {"GB", 55.4, -3.4},
    {"GE", 42.3, 43.4}, {"GH", 7.9, -1.0}, {"GR", 39.1, 21.8}, {"GT", 15.8, -90.2},
    {"HK", 22.3, 114.2}, {"HU", 47.2, 19.5}, {"ID", -0.8, 113.9}, {"IE", 53.4, -8.2},
    {"IL", 31.0, 34.9}, {"IN", 22.6, 79.0}, {"IQ", 33.2, 43.7}, {"IS", 64.9, -19.0},
    {"IT", 41.9, 12.6}, {"JP", 36.2, 138.3}, {"KE", 0.2, 37.9}, {"KG", 41.2, 74.8},
    {"KR", 35.9, 127.8}, {"KZ", 48.0, 66.9}, {"LI", 47.2, 9.6}, {"LT", 55.2, 23.9},
    {"LU", 49.8, 6.1}, {"LV", 56.9, 24.6}, {"MA", 31.8, -7.1}, {"MD", 47.4, 28.5},
    {"MK", 41.6, 21.7}, {"MM", 21.9, 95.9}, {"MN", 46.9, 103.8}, {"MX", 23.6, -102.6},
    {"MY", 4.2, 101.9}, {"NG", 9.1, 8.7}, {"NL", 52.1, 5.3}, {"NO", 60.5, 8.5},
    {"NZ", -41.0, 174.9}, {"PA", 8.5, -80.8}, {"PE", -9.2, -75.0}, {"PH", 12.9, 121.8},
    {"PK", 30.4, 69.3}, {"PL", 51.9, 19.1}, {"PT", 39.4, -8.2}, {"RO", 45.9, 24.9},
    {"RS", 44.0, 21.0}, {"RU", 61.5, 105.3}, {"SA", 23.9, 45.1}, {"SE", 60.1, 18.6},
    {"SG", 1.4, 103.8}, {"SI", 46.1, 14.8}, {"SK", 48.7, 19.7}, {"SN", 14.5, -14.5},
    {"TH", 15.9, 100.9}, {"TJ", 38.9, 71.3}, {"TR", 38.9, 35.2}, {"TW", 23.7, 121.0},
    {"UA", 48.4, 31.2}, {"US", 39.8, -98.6}, {"UY", -32.5, -55.8}, {"UZ", 41.4, 64.6},
    {"VN", 14.1, 108.3}, {"ZA", -30.6, 22.9},
};

GeoCoords* s_instance = nullptr;
} // namespace

GeoCoords* GeoCoords::instance()
{
    if (s_instance == nullptr)
    {
        s_instance = new GeoCoords();
    }
    return s_instance;
}

GeoCoords* GeoCoords::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    GeoCoords* inst = instance();
    QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
    return inst;
}

GeoCoords::GeoCoords(QObject* parent)
    : QObject(parent)
{
}

QPointF GeoCoords::coordsFor(const QString& countryCode) const
{
    if (countryCode.length() != 2)
    {
        return {};
    }
    const QString code = countryCode.toUpper();
    for (const Centroid& c : CENTROIDS)
    {
        if (code == QLatin1StringView(c.code))
        {
            return QPointF(c.lat, c.lon);
        }
    }
    return {};
}

QString GeoCoords::userCountry() const
{
    return GeoUtils::detectUserCountry();
}