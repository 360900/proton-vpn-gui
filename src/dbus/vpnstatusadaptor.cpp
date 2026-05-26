#include "vpnstatusadaptor.h"

VpnStatusAdaptor::VpnStatusAdaptor(VpnManager* parent)
    : QDBusAbstractAdaptor(parent)
    , m_manager(parent)
{
    setAutoRelaySignals(false);
    connect(m_manager, &VpnManager::connectionStateChanged,
            this,      &VpnStatusAdaptor::onConnectionStateChanged);
}

QString VpnStatusAdaptor::status() const
{
    return stateToString(m_manager->currentState());
}

QString VpnStatusAdaptor::connectedServer() const
{
    return m_manager->connectedServer();
}

void VpnStatusAdaptor::onConnectionStateChanged(const VpnState state, const QString& /*info*/)
{
    emit StatusChanged(stateToString(state));
}

QString VpnStatusAdaptor::stateToString(const VpnState state)
{
    switch (state)
    {
        case VpnState::Disconnected:
            return QStringLiteral("disconnected");
        case VpnState::Connecting:
            return QStringLiteral("connecting");
        case VpnState::Connected:
            return QStringLiteral("connected");
        case VpnState::Disconnecting:
            return QStringLiteral("disconnecting");
        case VpnState::Error:
            return QStringLiteral("error");
        default:
            return QStringLiteral("unknown");
    }
}
