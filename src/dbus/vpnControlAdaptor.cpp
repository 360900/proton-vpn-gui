#include "vpnControlAdaptor.h"

#include "../core/debug.h"

VpnControlAdaptor::VpnControlAdaptor(VpnService* parent)
    : QDBusAbstractAdaptor(parent)
    , m_service(parent)
{
}

void VpnControlAdaptor::Connect(const QString& country, const QString& city)
{
    DBG_APP(QStringLiteral("D-Bus: Connect(country='%1', city='%2')").arg(country, city));
    m_service->connectVpn(country, city);
}

void VpnControlAdaptor::Disconnect()
{
    DBG_APP(QStringLiteral("D-Bus: Disconnect()"));
    m_service->disconnectVpn();
}

void VpnControlAdaptor::Raise()
{
    DBG_APP(QStringLiteral("D-Bus: Raise()"));
    emit raiseRequested();
}
