#pragma once

#include <QDBusAbstractAdaptor>
#include <QString>
#include "../core/vpnService.h"

/**
 * D-Bus adaptor that exposes the VPN connection status on the session bus.
 *
 * Service name : io.github._360900.ProtonVpnGui
 * Object path  : /io/github/360900/ProtonVpnGui
 * Interface    : io.github._360900.ProtonVpnGui.Status
 *
 * Properties (read-only):
 *   Status          – "unknown" | "disconnected" | "connecting" |
 *                     "connected" | "disconnecting" | "error"
 *   ConnectedServer – server string while connected (e.g. "US-NJ#189"),
 *                     empty when not connected
 *
 * Signals:
 *   StatusChanged(status: string)  – fired on every VPN state transition
 */
class VpnStatusAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github._360900.ProtonVpnGui.Status")

    Q_PROPERTY(QString Status          READ status)
    Q_PROPERTY(QString ConnectedServer READ connectedServer)

public:
    explicit VpnStatusAdaptor(VpnService* parent);

    QString status()          const;
    QString connectedServer() const;

signals:
    void StatusChanged(const QString& status);

private:
    VpnService* m_service; // non-owning; the adaptor is parented to it
};
