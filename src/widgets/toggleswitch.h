#pragma once

#include <QWidget>

// ---------------------------------------------------------------------------
// ToggleSwitch – animated on/off switch
// ---------------------------------------------------------------------------
class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPos READ knobPos WRITE setKnobPos)

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    [[nodiscard]] bool isOn() const { return m_on; }
    void setOn(bool on, bool animate = true);

    [[nodiscard]] qreal knobPos() const { return m_knobPos; }

    void setKnobPos(const qreal v)
    {
        m_knobPos = v;
        update();
    }

    [[nodiscard]] QSize sizeHint() const override { return {44, 24}; }

signals:
    void toggled(bool on);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    bool m_on = false;
    qreal m_knobPos = 0.0;
    class QPropertyAnimation* m_anim;
};

