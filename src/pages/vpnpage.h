#pragma once

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QPropertyAnimation>
#include "../vpnmanager.h"

// ---------------------------------------------------------------------------
// PowerButton – circular power-icon button with an animated ring
// ---------------------------------------------------------------------------
class PowerButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal spinAngle READ spinAngle WRITE setSpinAngle)

public:
    explicit PowerButton(QWidget* parent = nullptr);

    enum class RingState { Unknown, Connected, Disconnected, Spinning };

    void setState(RingState s);

    [[nodiscard]] qreal spinAngle() const { return m_spinAngle; }

    void setSpinAngle(qreal a)
    {
        m_spinAngle = a;
        update();
    }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    RingState m_state = RingState::Unknown;
    qreal m_spinAngle = 0.0;
    bool m_hovered = false;
    QPropertyAnimation* m_anim = nullptr;

    void startSpin() const;
    void stopSpin();
};

// ---------------------------------------------------------------------------
// VpnPage
// ---------------------------------------------------------------------------
class VpnPage : public QWidget
{
    Q_OBJECT

public:
    explicit VpnPage(VpnManager* manager, QWidget* parent = nullptr);

    void onStateChanged(VpnState state, const QString& info);

signals:
    void connectRequested();
    void disconnectRequested();

private:
    VpnManager* m_manager;

    PowerButton* m_powerBtn;
    QLabel* m_statusLabel;
    QLabel* m_infoLabel;
    QLabel* m_timerLabel;
    QTimer* m_elapsedTimer;
    QTimer* m_checkingSpinnerTimer;
    int m_elapsedSeconds = 0;
    int m_checkingSpinnerFrame = 0;

    VpnState m_currentState = VpnState::Unknown;

    void updateUi(VpnState state, const QString& info);
    void startElapsedTimer();
    void stopElapsedTimer() const;
};

