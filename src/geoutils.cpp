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
// countryCodeToName
// ---------------------------------------------------------------------------
QString countryCodeToName(const QString& code)
{
    static const QMap<QString, QString> kCodeToName = {
        {QStringLiteral("AF"), QStringLiteral("Afghanistan")},
        {QStringLiteral("AL"), QStringLiteral("Albania")},
        {QStringLiteral("DZ"), QStringLiteral("Algeria")},
        {QStringLiteral("AO"), QStringLiteral("Angola")},
        {QStringLiteral("AR"), QStringLiteral("Argentina")},
        {QStringLiteral("AM"), QStringLiteral("Armenia")},
        {QStringLiteral("AU"), QStringLiteral("Australia")},
        {QStringLiteral("AT"), QStringLiteral("Austria")},
        {QStringLiteral("AZ"), QStringLiteral("Azerbaijan")},
        {QStringLiteral("BH"), QStringLiteral("Bahrain")},
        {QStringLiteral("BD"), QStringLiteral("Bangladesh")},
        {QStringLiteral("BY"), QStringLiteral("Belarus")},
        {QStringLiteral("BE"), QStringLiteral("Belgium")},
        {QStringLiteral("BT"), QStringLiteral("Bhutan")},
        {QStringLiteral("BA"), QStringLiteral("Bosnia and Herzegovina")},
        {QStringLiteral("BR"), QStringLiteral("Brazil")},
        {QStringLiteral("BN"), QStringLiteral("Brunei")},
        {QStringLiteral("BG"), QStringLiteral("Bulgaria")},
        {QStringLiteral("KH"), QStringLiteral("Cambodia")},
        {QStringLiteral("CM"), QStringLiteral("Cameroon")},
        {QStringLiteral("CA"), QStringLiteral("Canada")},
        {QStringLiteral("TD"), QStringLiteral("Chad")},
        {QStringLiteral("CL"), QStringLiteral("Chile")},
        {QStringLiteral("CO"), QStringLiteral("Colombia")},
        {QStringLiteral("KM"), QStringLiteral("Comoros")},
        {QStringLiteral("CR"), QStringLiteral("Costa Rica")},
        {QStringLiteral("HR"), QStringLiteral("Croatia")},
        {QStringLiteral("CU"), QStringLiteral("Cuba")},
        {QStringLiteral("CY"), QStringLiteral("Cyprus")},
        {QStringLiteral("CZ"), QStringLiteral("Czech Republic")},
        {QStringLiteral("DK"), QStringLiteral("Denmark")},
        {QStringLiteral("DO"), QStringLiteral("Dominican Republic")},
        {QStringLiteral("EC"), QStringLiteral("Ecuador")},
        {QStringLiteral("EG"), QStringLiteral("Egypt")},
        {QStringLiteral("SV"), QStringLiteral("El Salvador")},
        {QStringLiteral("ER"), QStringLiteral("Eritrea")},
        {QStringLiteral("EE"), QStringLiteral("Estonia")},
        {QStringLiteral("ET"), QStringLiteral("Ethiopia")},
        {QStringLiteral("FI"), QStringLiteral("Finland")},
        {QStringLiteral("FR"), QStringLiteral("France")},
        {QStringLiteral("GE"), QStringLiteral("Georgia")},
        {QStringLiteral("DE"), QStringLiteral("Germany")},
        {QStringLiteral("GH"), QStringLiteral("Ghana")},
        {QStringLiteral("GR"), QStringLiteral("Greece")},
        {QStringLiteral("GT"), QStringLiteral("Guatemala")},
        {QStringLiteral("HN"), QStringLiteral("Honduras")},
        {QStringLiteral("HK"), QStringLiteral("Hong Kong")},
        {QStringLiteral("HU"), QStringLiteral("Hungary")},
        {QStringLiteral("IS"), QStringLiteral("Iceland")},
        {QStringLiteral("IN"), QStringLiteral("India")},
        {QStringLiteral("ID"), QStringLiteral("Indonesia")},
        {QStringLiteral("IQ"), QStringLiteral("Iraq")},
        {QStringLiteral("IE"), QStringLiteral("Ireland")},
        {QStringLiteral("IL"), QStringLiteral("Israel")},
        {QStringLiteral("IT"), QStringLiteral("Italy")},
        {QStringLiteral("CI"), QStringLiteral("Ivory Coast")},
        {QStringLiteral("JP"), QStringLiteral("Japan")},
        {QStringLiteral("JO"), QStringLiteral("Jordan")},
        {QStringLiteral("KZ"), QStringLiteral("Kazakhstan")},
        {QStringLiteral("KE"), QStringLiteral("Kenya")},
        {QStringLiteral("KW"), QStringLiteral("Kuwait")},
        {QStringLiteral("LA"), QStringLiteral("Laos")},
        {QStringLiteral("LV"), QStringLiteral("Latvia")},
        {QStringLiteral("LY"), QStringLiteral("Libya")},
        {QStringLiteral("LT"), QStringLiteral("Lithuania")},
        {QStringLiteral("LU"), QStringLiteral("Luxembourg")},
        {QStringLiteral("MK"), QStringLiteral("Macedonia")},
        {QStringLiteral("MY"), QStringLiteral("Malaysia")},
        {QStringLiteral("MT"), QStringLiteral("Malta")},
        {QStringLiteral("MR"), QStringLiteral("Mauritania")},
        {QStringLiteral("MU"), QStringLiteral("Mauritius")},
        {QStringLiteral("MX"), QStringLiteral("Mexico")},
        {QStringLiteral("MD"), QStringLiteral("Moldova")},
        {QStringLiteral("MN"), QStringLiteral("Mongolia")},
        {QStringLiteral("ME"), QStringLiteral("Montenegro")},
        {QStringLiteral("MA"), QStringLiteral("Morocco")},
        {QStringLiteral("MZ"), QStringLiteral("Mozambique")},
        {QStringLiteral("MM"), QStringLiteral("Myanmar")},
        {QStringLiteral("NP"), QStringLiteral("Nepal")},
        {QStringLiteral("NL"), QStringLiteral("Netherlands")},
        {QStringLiteral("NZ"), QStringLiteral("New Zealand")},
        {QStringLiteral("NG"), QStringLiteral("Nigeria")},
        {QStringLiteral("NO"), QStringLiteral("Norway")},
        {QStringLiteral("OM"), QStringLiteral("Oman")},
        {QStringLiteral("PK"), QStringLiteral("Pakistan")},
        {QStringLiteral("PS"), QStringLiteral("Palestinian Territory")},
        {QStringLiteral("PA"), QStringLiteral("Panama")},
        {QStringLiteral("PE"), QStringLiteral("Peru")},
        {QStringLiteral("PH"), QStringLiteral("Philippines")},
        {QStringLiteral("PL"), QStringLiteral("Poland")},
        {QStringLiteral("PT"), QStringLiteral("Portugal")},
        {QStringLiteral("PR"), QStringLiteral("Puerto Rico")},
        {QStringLiteral("QA"), QStringLiteral("Qatar")},
        {QStringLiteral("RO"), QStringLiteral("Romania")},
        {QStringLiteral("RU"), QStringLiteral("Russia")},
        {QStringLiteral("RW"), QStringLiteral("Rwanda")},
        {QStringLiteral("SA"), QStringLiteral("Saudi Arabia")},
        {QStringLiteral("SN"), QStringLiteral("Senegal")},
        {QStringLiteral("RS"), QStringLiteral("Serbia")},
        {QStringLiteral("SG"), QStringLiteral("Singapore")},
        {QStringLiteral("SK"), QStringLiteral("Slovakia")},
        {QStringLiteral("SI"), QStringLiteral("Slovenia")},
        {QStringLiteral("SO"), QStringLiteral("Somalia")},
        {QStringLiteral("ZA"), QStringLiteral("South Africa")},
        {QStringLiteral("KR"), QStringLiteral("South Korea")},
        {QStringLiteral("SS"), QStringLiteral("South Sudan")},
        {QStringLiteral("ES"), QStringLiteral("Spain")},
        {QStringLiteral("LK"), QStringLiteral("Sri Lanka")},
        {QStringLiteral("SD"), QStringLiteral("Sudan")},
        {QStringLiteral("SE"), QStringLiteral("Sweden")},
        {QStringLiteral("CH"), QStringLiteral("Switzerland")},
        {QStringLiteral("SY"), QStringLiteral("Syria")},
        {QStringLiteral("TW"), QStringLiteral("Taiwan")},
        {QStringLiteral("TJ"), QStringLiteral("Tajikistan")},
        {QStringLiteral("TZ"), QStringLiteral("Tanzania")},
        {QStringLiteral("TH"), QStringLiteral("Thailand")},
        {QStringLiteral("TG"), QStringLiteral("Togo")},
        {QStringLiteral("TN"), QStringLiteral("Tunisia")},
        {QStringLiteral("TR"), QStringLiteral("Turkey")},
        {QStringLiteral("TM"), QStringLiteral("Turkmenistan")},
        {QStringLiteral("UG"), QStringLiteral("Uganda")},
        {QStringLiteral("UA"), QStringLiteral("Ukraine")},
        {QStringLiteral("AE"), QStringLiteral("United Arab Emirates")},
        {QStringLiteral("GB"), QStringLiteral("United Kingdom")},
        {QStringLiteral("UK"), QStringLiteral("United Kingdom")},
        {QStringLiteral("US"), QStringLiteral("United States")},
        {QStringLiteral("UZ"), QStringLiteral("Uzbekistan")},
        {QStringLiteral("VE"), QStringLiteral("Venezuela")},
        {QStringLiteral("VN"), QStringLiteral("Vietnam")},
        {QStringLiteral("YE"), QStringLiteral("Yemen")},
    };
    const auto it = kCodeToName.find(code.toUpper());
    return it != kCodeToName.end() ? it.value() : code;
}

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

