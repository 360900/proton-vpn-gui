// vpnmanager.cpp
// VpnManager: construction, settings file access, and background polling
// infrastructure.
//
// All functions that interact with the protonvpn CLI live in protonvpncli.cpp.

#include "vpnmanager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

// Path to the ProtonVPN settings file, relative to the user's home directory.
static const QString kSettingsPath =
    QDir::homePath() + QStringLiteral("/.config/Proton/VPN/settings.json");

VpnManager::VpnManager(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// Settings (file-based — no CLI involved)
// ---------------------------------------------------------------------------

void VpnManager::fetchSettings()
{
    QMap<QString, QString> settings;

    QFile f(kSettingsPath);
    if (!f.open(QIODevice::ReadOnly))
    {
        emit settingsReady(settings);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    if (!doc.isObject())
    {
        emit settingsReady(settings);
        return;
    }

    const QJsonObject root = doc.object();

    auto boolStr = [](bool b) -> QString
    {
        return b ? QStringLiteral("on") : QStringLiteral("off");
    };

    if (root.contains(QStringLiteral("killswitch")))
    {
        const int ks = root[QStringLiteral("killswitch")].toInt(0);
        settings.insert(QStringLiteral("kill-switch"),
                        ks == 1 ? QStringLiteral("standard") : QStringLiteral("off"));
    }

    if (root.contains(QStringLiteral("ipv6")))
        settings.insert(QStringLiteral("ipv6"),
                        boolStr(root[QStringLiteral("ipv6")].toBool()));

    if (root.contains(QStringLiteral("anonymous_crash_reports")))
        settings.insert(QStringLiteral("anonymous-crash-reports"),
                        boolStr(root[QStringLiteral("anonymous_crash_reports")].toBool()));

    if (root.contains(QStringLiteral("custom_dns")))
    {
        const QJsonObject dns = root[QStringLiteral("custom_dns")].toObject();
        const bool dnsEnabled = dns[QStringLiteral("enabled")].toBool(false);
        if (dnsEnabled)
        {
            const QJsonArray ipList = dns[QStringLiteral("ip_list")].toArray();
            QStringList ips;
            for (const auto& v : ipList) ips << v.toString();
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
            const int ns = feat[QStringLiteral("netshield")].toInt(0);
            QString nsVal;
            switch (ns)
            {
            case 1:  nsVal = QStringLiteral("malware-only");         break;
            case 2:  nsVal = QStringLiteral("malware-ads-trackers"); break;
            default: nsVal = QStringLiteral("off");                  break;
            }
            settings.insert(QStringLiteral("netshield"), nsVal);
        }

        if (feat.contains(QStringLiteral("moderate_nat")))
            settings.insert(QStringLiteral("moderate-nat"),
                            boolStr(feat[QStringLiteral("moderate_nat")].toBool()));

        if (feat.contains(QStringLiteral("vpn_accelerator")))
            settings.insert(QStringLiteral("vpn-accelerator"),
                            boolStr(feat[QStringLiteral("vpn_accelerator")].toBool()));

        if (feat.contains(QStringLiteral("port_forwarding")))
            settings.insert(QStringLiteral("port-forwarding"),
                            boolStr(feat[QStringLiteral("port_forwarding")].toBool()));
    }

    emit settingsReady(settings);
}

bool VpnManager::portForwardingEnabled() const
{
    QFile f(kSettingsPath);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return false;
    return doc.object()
               .value(QStringLiteral("features")).toObject()
               .value(QStringLiteral("port_forwarding")).toBool(false);
}

// ---------------------------------------------------------------------------
// Background polling
// ---------------------------------------------------------------------------

void VpnManager::startPolling()
{
    if (m_pollTimer)
        return;

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(15'000); // 15 s
    connect(m_pollTimer, &QTimer::timeout, this, &VpnManager::pollStatus);
    m_pollTimer->start();
}

void VpnManager::stopPolling()
{
    if (m_pollTimer)
    {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    m_pollActive = false;
}
