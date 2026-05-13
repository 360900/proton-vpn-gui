#include "toggleswitch.h"

#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QPainter>

// ============================================================
// ToggleSwitch
// ============================================================

ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent)
{
    setFixedSize(ToggleSwitch::sizeHint());
    setCursor(Qt::PointingHandCursor);
    m_anim = new QPropertyAnimation(this, "knobPos", this);
    m_anim->setDuration(150);
    m_anim->setEasingCurve(QEasingCurve::InOutQuad);
}

void ToggleSwitch::setOn(const bool on, const bool animate)
{
    if (m_on == on) return;
    m_on = on;

    m_anim->stop();

    if (animate)
    {
        m_anim->setStartValue(m_knobPos);
        m_anim->setEndValue(on ? 1.0 : 0.0);
        m_anim->start();
    }
    else
    {
        m_knobPos = on ? 1.0 : 0.0;
        update();
    }
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool enabled = isEnabled();
    const int w = width(), h = height(), r = h / 2;
    if (enabled)
    {
        constexpr QColor trackOn(0x6d, 0x4a, 0xff), trackOff(0x55, 0x55, 0x66);
        QColor track;
        track.setRed(static_cast<int>(trackOff.red() + (trackOn.red() - trackOff.red()) * m_knobPos));
        track.setGreen(static_cast<int>(trackOff.green() + (trackOn.green() - trackOff.green()) * m_knobPos));
        track.setBlue(static_cast<int>(trackOff.blue() + (trackOn.blue() - trackOff.blue()) * m_knobPos));
        p.setBrush(track);
    }
    else
    {
        p.setBrush(QColor(0x3a, 0x3a, 0x44));
    }
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, w, h, r, r);
    const int knobD = h - 4, knobMin = 2, knobMax = w - knobD - 2;
    const int knobX = static_cast<int>(knobMin + (knobMax - knobMin) * m_knobPos);
    p.setBrush(enabled ? Qt::white : QColor(0x66, 0x66, 0x77));
    p.drawEllipse(knobX, 2, knobD, knobD);
}

void ToggleSwitch::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
    {
        setOn(!m_on);
        emit toggled(m_on);
    }
    QWidget::mousePressEvent(e);
}

