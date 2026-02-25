#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include "../vpnmanager.h"

class VpnPage : public QWidget
{
    Q_OBJECT
public:
    explicit VpnPage(VpnManager *manager, QWidget *parent = nullptr);

    void onStateChanged(VpnState state, const QString &info);

signals:
    void connectRequested();
    void disconnectRequested();

private:
    VpnManager *m_manager;

    QLabel *m_stateIconLabel;
    QLabel *m_statusLabel;
    QLabel *m_infoLabel;
    QPushButton *m_connectBtn;
    QLabel *m_timerLabel;
    QTimer *m_elapsedTimer;
    QTimer *m_checkingSpinnerTimer;
    int m_elapsedSeconds = 0;
    int m_checkingSpinnerFrame = 0;

    VpnState m_currentState = VpnState::Unknown;

    void updateUi(VpnState state, const QString &info);
    void startElapsedTimer();
    void stopElapsedTimer();
};

