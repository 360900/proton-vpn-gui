// vpnmanager.cpp
// VpnManager: construction, settings file access, and background status
// monitor infrastructure.
//
// All functions that interact with the protonvpn CLI live in protonvpncli.cpp.

#include "vpnmanager.h"
#include "cli/statusmonitor.h"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>

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
// Background status monitor (long-lived subprocess, every 15 s while logged in)
// ---------------------------------------------------------------------------

void VpnManager::startStatusMonitor()
{
    if (m_statusMonitor)
        return;

    m_statusMonitor = new StatusMonitor(this);
    connect(m_statusMonitor, &StatusMonitor::statusParsed,
            this, [this](const QMap<QString, QString>& fields)
            {
                if (m_state == VpnState::Connecting || m_state == VpnState::Disconnecting)
                    return; // ignore polls during in-progress transitions
                applyStatusFields(fields);
            });

    m_statusMonitor->start();
}

void VpnManager::stopStatusMonitor()
{
    if (!m_statusMonitor)
        return;

    m_statusMonitor->stop();
    m_statusMonitor->deleteLater();
    m_statusMonitor = nullptr;
}

// ---------------------------------------------------------------------------
// Apply a parsed `protonvpn status` snapshot to internal state.
// Only emits signals when state or connected server actually changed.
// ---------------------------------------------------------------------------

void VpnManager::applyStatusFields(const QMap<QString, QString>& fields)
{
    const QString statusVal = fields.value(QStringLiteral("status")).toLower();
    const VpnState newState = (statusVal == QStringLiteral("connected"))
                              ? VpnState::Connected
                              : VpnState::Disconnected;

    const QString server = (newState == VpnState::Connected)
                           ? fields.value(QStringLiteral("server"))
                           : QString();
    const QString city   = StatusMonitor::parseCityFromServer(server);
    const QString info   = server.isEmpty()
                           ? QString()
                           : QStringLiteral("Connected to %1.").arg(server);

    // Country code — e.g. "US" from "US-NJ#203 in Secaucus, United States".
    QString countryCode;
    if (!server.isEmpty())
    {
        const int dashPos = server.indexOf(QLatin1Char('-'));
        const int hashPos = server.indexOf(QLatin1Char('#'));
        const int endPos  = (dashPos >= 0 && (hashPos < 0 || dashPos < hashPos))
                            ? dashPos : hashPos;
        if (endPos > 0)
            countryCode = server.left(endPos).toUpper();
    }

#ifdef QT_DEBUG
    const VpnState dbgPrevState  = m_state;
    const QString  dbgPrevServer = m_connectedServer;
#endif

    const bool stateChanged  = newState != m_state;
    const bool serverChanged = !stateChanged &&
                               newState == VpnState::Connected &&
                               !m_connectedServer.isEmpty() &&
                               server != m_connectedServer;

    if (stateChanged || serverChanged)
    {
        m_state           = newState;
        m_connectedServer = (newState == VpnState::Connected) ? server : QString();

        if (newState == VpnState::Connected)
        {
            if (!city.isEmpty())        emit connectionCityKnown(city);
            if (!countryCode.isEmpty()) emit connectionCountryKnown(countryCode);
            emit connectionStateChanged(m_state, info);
        }
        else
        {
            emit connectionStateChanged(m_state, QString());
        }
    }
    else if (newState == VpnState::Connected && m_connectedServer.isEmpty())
    {
        m_connectedServer = server;
    }

#ifdef QT_DEBUG
    {
        auto stateToStr = [](const VpnState s) -> const char*
        {
            switch (s)
            {
                case VpnState::Connected:     return "Connected";
                case VpnState::Disconnected:  return "Disconnected";
                case VpnState::Connecting:    return "Connecting";
                case VpnState::Disconnecting: return "Disconnecting";
                default:                      return "Error";
            }
        };
        const bool dbgStateChanged  = newState != dbgPrevState;
        const bool dbgServerChanged = !dbgStateChanged &&
                                      newState == VpnState::Connected &&
                                      !dbgPrevServer.isEmpty() &&
                                      server != dbgPrevServer;
        if (dbgStateChanged)
            qDebug("[Status Polling] State changed:  %s → %s",
                   stateToStr(dbgPrevState), stateToStr(newState));
        else if (dbgServerChanged)
            qDebug("[Status Polling] Server changed: \"%s\" → \"%s\"",
                   qUtf8Printable(dbgPrevServer), qUtf8Printable(server));
    }
#endif
}


