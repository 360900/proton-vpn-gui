#pragma once

#include <QDBusAbstractAdaptor>
#include <QString>
#include "../core/vpnService.h"

/**
 * D-Bus adaptor that accepts VPN commands on the session bus.
 *
 * Service name : io.github._360900.ProtonVpnGui
 * Object path  : /io/github/360900/ProtonVpnGui
 * Interface    : io.github._360900.ProtonVpnGui.Control
 *
 * Methods:
 *   Connect(country: string, city: string) - connect; empty strings mean
 *                                            "fastest server"
 *   Disconnect()                           - disconnect
 *   Raise()                                - show and focus the main window
 *                                            (used by second instances)
 */
class VpnControlAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github._360900.ProtonVpnGui.Control")

public:
    explicit VpnControlAdaptor(VpnService* parent);

public slots:
    void Connect(const QString& country, const QString& city);
    void Disconnect();
    void Raise();

signals:
    // Emitted by Raise(); main.cpp connects this to the main window.
    void raiseRequested();

private:
    VpnService* m_service; // non-owning; the adaptor is parented to it
};
