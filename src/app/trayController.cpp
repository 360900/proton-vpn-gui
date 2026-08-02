// trayController.cpp
// See trayController.h.

#include "trayController.h"

#include "../appConfig.h"
#include "vpnFacade.h"

#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QSvgRenderer>

namespace
{
constexpr int TRAY_ICON_SIZE = 22;
constexpr int NOTIFICATION_ICON_SIZE = 64;
constexpr int NOTIFICATION_TIMEOUT_MS = 5'000;

QString stateIconResource(const VpnState state)
{
    switch (state)
    {
        case VpnState::Connected:
            return QStringLiteral(":/assets/state-connected.svg");
        case VpnState::Connecting:
        case VpnState::Disconnecting:
            return QStringLiteral(":/assets/state-connecting.svg");
        case VpnState::Error:
            return QStringLiteral(":/assets/state-error.svg");
        default:
            return QStringLiteral(":/assets/state-disconnected.svg");
    }
}
} // namespace

TrayController::TrayController(QObject* parent)
    : QObject(parent)
    , m_trayIcon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    QAction* showAction = m_menu->addAction(tr("Show"));
    connect(showAction, &QAction::triggered, this, &TrayController::showRequested);

    m_toggleAction = m_menu->addAction(tr("Connect"));
    connect(m_toggleAction, &QAction::triggered, this, &TrayController::connectToggleRequested);

    m_menu->addSeparator();
    QAction* quitAction = m_menu->addAction(tr("Quit"));
    connect(quitAction, &QAction::triggered, this, [this]
    {
        const VpnState state = VpnFacade::instance()->service()->state();
        if (state == VpnState::Connected || state == VpnState::Connecting)
        {
            emit quitConfirmationRequested();
        }
        else
        {
            QApplication::quit();
        }
    });

    m_trayIcon->setContextMenu(m_menu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
            [this](const QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger)
                {
                    emit showRequested();
                }
            });

    updateState(VpnState::Unknown);
    m_trayIcon->show();
}

void TrayController::updateState(const VpnState state)
{
    m_trayIcon->setIcon(stateIcon(state));

    switch (state)
    {
        case VpnState::Connected:
            m_trayIcon->setToolTip(tr("Proton VPN - Connected"));
            m_toggleAction->setText(tr("Disconnect"));
            m_toggleAction->setEnabled(true);
            break;
        case VpnState::Connecting:
            m_trayIcon->setToolTip(tr("Proton VPN - Connecting…"));
            m_toggleAction->setText(tr("Disconnect"));
            m_toggleAction->setEnabled(true);
            break;
        case VpnState::Disconnecting:
            m_trayIcon->setToolTip(tr("Proton VPN - Disconnecting…"));
            m_toggleAction->setText(tr("Connect"));
            m_toggleAction->setEnabled(false);
            break;
        case VpnState::Error:
            m_trayIcon->setToolTip(tr("Proton VPN - Connection error"));
            m_toggleAction->setText(tr("Connect"));
            m_toggleAction->setEnabled(true);
            break;
        default:
            m_trayIcon->setToolTip(tr("Proton VPN - Disconnected"));
            m_toggleAction->setText(tr("Connect"));
            m_toggleAction->setEnabled(true);
            break;
    }
}

void TrayController::notify(const QString& title, const QString& message) const
{
    if (AppConfig::instance().notifications() == false)
    {
        return;
    }
    QSvgRenderer renderer(QStringLiteral(":/assets/proton-vpn-gui.svg"));
    QPixmap iconPix(NOTIFICATION_ICON_SIZE, NOTIFICATION_ICON_SIZE);
    iconPix.fill(Qt::transparent);
    QPainter p(&iconPix);
    renderer.render(&p);
    p.end();
    m_trayIcon->showMessage(title, message, QIcon(iconPix), NOTIFICATION_TIMEOUT_MS);
}

QIcon TrayController::stateIcon(const VpnState state) const
{
    QSvgRenderer renderer(stateIconResource(state));
    const qreal dpr = qApp->devicePixelRatio();
    QPixmap pix(QSize(TRAY_ICON_SIZE, TRAY_ICON_SIZE) * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    renderer.render(&painter, QRectF(0, 0, TRAY_ICON_SIZE, TRAY_ICON_SIZE));
    painter.end();
    return QIcon(pix);
}
