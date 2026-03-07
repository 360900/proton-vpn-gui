#include "geoutils.h"

#include <QFile>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QTimeZone>

namespace GeoUtils
{

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
// detectUserCountry
// ---------------------------------------------------------------------------
QString detectUserCountry()
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        const QString code = QLocale::territoryToCode(territory);
        if (!code.isEmpty())
            return code.toUpper();
#endif
        // Fallback: parse the BCP-47 name (e.g. "en-US" → "US")
        const QString bcp47 = sysLocale.bcp47Name();
        const qsizetype dashPos = bcp47.indexOf(QLatin1Char('-'));
        if (dashPos != -1)
        {
            const QString regionTag = bcp47.mid(dashPos + 1).toUpper();
            if (regionTag.length() == 2 && regionTag[0].isLetter() && regionTag[1].isLetter())
                return regionTag;
        }
    }

    return {};
}

// ---------------------------------------------------------------------------
// svgPixmap
// ---------------------------------------------------------------------------
QPixmap svgPixmap(const QString& resourcePath, int size)
{
    QSvgRenderer renderer(resourcePath);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return pixmap;
}

// ---------------------------------------------------------------------------
// flagIcon
// ---------------------------------------------------------------------------
QIcon flagIcon(const QString& countryCode)
{
    static QMap<QString, QIcon> cache;
    const QString key = countryCode.toLower();
    auto it = cache.find(key);
    if (it != cache.end())
        return it.value();

    const QString path = QStringLiteral(":/flags/") + key;
    if (!QFile::exists(path))
    {
        cache.insert(key, QIcon());
        return {};
    }

    const QIcon icon(svgPixmap(path, 20));
    cache.insert(key, icon);
    return icon;
}

} // namespace GeoUtils

