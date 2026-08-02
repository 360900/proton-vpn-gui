// protonCliSettings.cpp
// See protonCliSettings.h.

#include "protonCliSettings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace
{
// Integer enums used by the CLI's settings file.
constexpr int KILLSWITCH_MODE_STANDARD = 1;
constexpr int NETSHIELD_MODE_MALWARE   = 1;
constexpr int NETSHIELD_MODE_FULL      = 2;

QString boolStr(const bool b)
{
    return b ? QStringLiteral("on") : QStringLiteral("off");
}
} // namespace

namespace ProtonCliSettings
{

// The CLI resolves its config dir through $XDG_CONFIG_HOME (via pyxdg).
// Use the same base so the GUI reads the settings file the CLI writes:
// natively this is ~/.config, under Flatpak it is the app-private config dir.
QString settingsFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/Proton/VPN/settings.json");
}

QMap<QString, QString> parseSettingsJson(const QByteArray& json)
{
    QMap<QString, QString> settings;

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (doc.isObject() == false)
    {
        return settings;
    }
    const QJsonObject root = doc.object();

    if (root.contains(QStringLiteral("killswitch")))
    {
        const int ks = root[QStringLiteral("killswitch")].toInt(0);
        settings.insert(QStringLiteral("kill-switch"),
                        ks == KILLSWITCH_MODE_STANDARD ? QStringLiteral("standard")
                                                       : QStringLiteral("off"));
    }

    if (root.contains(QStringLiteral("ipv6")))
    {
        settings.insert(QStringLiteral("ipv6"), boolStr(root[QStringLiteral("ipv6")].toBool()));
    }

    if (root.contains(QStringLiteral("anonymous_crash_reports")))
    {
        settings.insert(QStringLiteral("anonymous-crash-reports"),
                        boolStr(root[QStringLiteral("anonymous_crash_reports")].toBool()));
    }

    if (root.contains(QStringLiteral("custom_dns")))
    {
        const QJsonObject dns = root[QStringLiteral("custom_dns")].toObject();
        if (dns[QStringLiteral("enabled")].toBool(false))
        {
            const QJsonArray ipList = dns[QStringLiteral("ip_list")].toArray();
            QStringList ips;
            for (const auto& v : ipList)
            {
                ips << v.toString();
            }
            settings.insert(QStringLiteral("custom-dns"),
                            ips.isEmpty() ? QStringLiteral("on") : ips.join(QLatin1Char(',')));
        }
        else
        {
            settings.insert(QStringLiteral("custom-dns"), QStringLiteral("off"));
        }
    }

    if (root.contains(QStringLiteral("features")))
    {
        const QJsonObject feat = root[QStringLiteral("features")].toObject();

        if (feat.contains(QStringLiteral("netshield")))
        {
            QString nsVal;
            switch (feat[QStringLiteral("netshield")].toInt(0))
            {
                case NETSHIELD_MODE_MALWARE:
                    nsVal = QStringLiteral("malware-only");
                    break;
                case NETSHIELD_MODE_FULL:
                    nsVal = QStringLiteral("malware-ads-trackers");
                    break;
                default:
                    nsVal = QStringLiteral("off");
                    break;
            }
            settings.insert(QStringLiteral("netshield"), nsVal);
        }

        if (feat.contains(QStringLiteral("moderate_nat")))
        {
            settings.insert(QStringLiteral("moderate-nat"),
                            boolStr(feat[QStringLiteral("moderate_nat")].toBool()));
        }

        if (feat.contains(QStringLiteral("vpn_accelerator")))
        {
            settings.insert(QStringLiteral("vpn-accelerator"),
                            boolStr(feat[QStringLiteral("vpn_accelerator")].toBool()));
        }

        if (feat.contains(QStringLiteral("port_forwarding")))
        {
            settings.insert(QStringLiteral("port-forwarding"),
                            boolStr(feat[QStringLiteral("port_forwarding")].toBool()));
        }
    }

    return settings;
}

QMap<QString, QString> readSettings()
{
    QFile f(settingsFilePath());
    if (f.open(QIODevice::ReadOnly) == false)
    {
        return {};
    }
    return parseSettingsJson(f.readAll());
}

bool portForwardingEnabled()
{
    QFile f(settingsFilePath());
    if (f.open(QIODevice::ReadOnly) == false)
    {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject() == false)
    {
        return false;
    }
    return doc.object()
               .value(QStringLiteral("features")).toObject()
               .value(QStringLiteral("port_forwarding")).toBool(false);
}

} // namespace ProtonCliSettings
