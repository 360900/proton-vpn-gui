#include "vpnStatusAdaptor.h"

#include "../core/cliParsers.h"

VpnStatusAdaptor::VpnStatusAdaptor(VpnService* parent)
    : QDBusAbstractAdaptor(parent)
    , m_service(parent)
{
    setAutoRelaySignals(false);
    connect(m_service, &VpnService::stateChanged, this,
            [this](const VpnState state, const QString&)
            {
                emit StatusChanged(CliParsers::vpnStateToString(state));
            });
}

QString VpnStatusAdaptor::status() const
{
    return CliParsers::vpnStateToString(m_service->state());
}

QString VpnStatusAdaptor::connectedServer() const
{
    return m_service->connectedServer();
}
