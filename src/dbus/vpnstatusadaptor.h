#pragma once

#include <QDBusAbstractAdaptor>
#include <QString>
#include "../vpnmanager.h"

/**
 * D-Bus adaptor that exposes the VPN connection status on the session bus.
 *
 * Service name : com.protonvpn.app
 * Object path  : /com/protonvpn/app
 * Interface    : com.protonvpn.app.Status
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
    Q_CLASSINFO("D-Bus Interface", "com.protonvpn.app.Status")

    Q_PROPERTY(QString Status          READ status)
    Q_PROPERTY(QString ConnectedServer READ connectedServer)

public:
    explicit VpnStatusAdaptor(VpnManager* parent);

    QString status()          const;
    QString connectedServer() const;

signals:
    void StatusChanged(const QString& status);

private slots:
    void onConnectionStateChanged(VpnState state, const QString& info);

private:
    VpnManager* m_manager; // non-owning; lifetime is managed by MainWindow
    static QString stateToString(VpnState state);
};

