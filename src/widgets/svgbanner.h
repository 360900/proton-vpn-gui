#pragma once

#include <QPainter>
#include <QSvgRenderer>
#include <QWidget>

// ============================================================
// SvgBanner – responsive SVG widget that maintains a fixed
// aspect ratio and fills its parent's width up to a maximum.
// Once the maximum width is reached the widget stops expanding
// and stays centered via the parent layout's alignment flag.
//
// aspectRatio = width / height  (e.g. 4.0 for a 4:1 banner)
// ============================================================
class SvgBanner : public QWidget
{
public:
    explicit SvgBanner(const QString& resource,
                       const qreal aspectRatio,
                       QWidget* parent = nullptr)
        : QWidget(parent), m_renderer(resource), m_aspect(aspectRatio)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QWidget::setMaximumWidth(defaultMaxWidth); // start with a reasonable default max width
        m_maxWidth = defaultMaxWidth;
    }

    void setMaxWidth(const int maxWidth) // In pixels
    {
        QWidget::setMaximumWidth(maxWidth);
        m_maxWidth  = maxWidth;
        updateGeometry();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int parentW = parentWidget() ? parentWidget()->width() : m_maxWidth;
        const int w = qMin(parentW, m_maxWidth);
        return {w, qRound(w / m_aspect)};
    }

    [[nodiscard]] int heightForWidth(int w) const override
    {
        return qRound(qMin(w, m_maxWidth) / m_aspect);
    }

    [[nodiscard]] bool hasHeightForWidth() const override { return true; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        m_renderer.render(&p, QRectF(rect()));
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        // Lock height to the aspect-ratio-correct value for the current width
        // so the layout can never squeeze the two axes independently.
        const int correctH = qRound(width() / m_aspect);
        if (height() != correctH)
            setFixedHeight(correctH);
        updateGeometry();
    }

private:
    int defaultMaxWidth = 500;

    QSvgRenderer m_renderer;
    qreal m_aspect;
    int   m_maxWidth;
};
