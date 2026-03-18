#pragma once

#include <QPainter>
#include <QSvgRenderer>
#include <QWidget>

// ============================================================
// SvgBanner – responsive SVG widget that maintains a fixed
// aspect ratio and fills its parent's width up to a maximum.
//
// aspectRatio = width / height  (e.g. 4.0 for a 4:1 banner)
// maxWidth    = maximum pixel width (default 320)
// ============================================================
class SvgBanner : public QWidget
{
public:
    explicit SvgBanner(const QString& resource,
                       const qreal aspectRatio,
                       QWidget* parent = nullptr)
        : QWidget(parent), m_renderer(resource), m_aspect(aspectRatio)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMaximumWidth(480);
    }

    void setMaxWidth(const int maxWidth) // In pixels
    {
        QWidget::setMaximumWidth(maxWidth);
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int w = qMin(width() > 0 ? width()
                                       : (parentWidget() ? parentWidget()->width() : 320),
                           maximumWidth());
        return {w, qRound(w / m_aspect)};
    }

    [[nodiscard]] int heightForWidth(int w) const override
    {
        return qRound(w / m_aspect);
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
        const int h = qRound(width() / m_aspect);
        if (height() != h)
            setFixedHeight(h);
    }

private:
    QSvgRenderer m_renderer;
    qreal m_aspect;
};


