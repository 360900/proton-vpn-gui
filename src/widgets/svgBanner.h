#pragma once

#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QSvgRenderer>
#include <QWidget>

// SVG widget that scales to fill its parent's width up to a configurable maximum,
// keeping a fixed aspect ratio (width / height, e.g. 4.0 for a 4:1 banner).
// Optionally call setLightResource() to supply an alternate SVG for light themes.
class SvgBanner : public QWidget
{
    static constexpr int DEFAULT_MAX_WIDTH               = 500;
    static constexpr int LIGHT_THEME_LIGHTNESS_THRESHOLD = 128;

public:
    explicit SvgBanner(const QString& resource,
                       const qreal aspectRatio,
                       QWidget* parent = nullptr)
        : QWidget(parent), m_renderer(resource), m_aspect(aspectRatio)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        QWidget::setMaximumWidth(DEFAULT_MAX_WIDTH);
        m_maxWidth = DEFAULT_MAX_WIDTH;
    }

    void setMaxWidth(const int maxWidth) // In pixels
    {
        QWidget::setMaximumWidth(maxWidth);
        m_maxWidth  = maxWidth;
        updateGeometry();
    }

    // Provide an alternate SVG resource to use when the background is light.
    // Pass an empty string to disable theme-switching.
    void setLightResource(const QString& resource)
    {
        m_lightRenderer = resource.isEmpty() ? nullptr : std::make_unique<QSvgRenderer>(resource);
        update();
    }

    [[nodiscard]] QSize sizeHint() const override
    {
        const int parentW = (parentWidget() != nullptr) ? parentWidget()->width() : m_maxWidth;
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

        QSvgRenderer& active = activeRenderer();
        active.render(&p, QRectF(rect()));
    }

    void resizeEvent(QResizeEvent* e) override
    {
        QWidget::resizeEvent(e);
        // Lock height to the aspect-ratio-correct value for the current width
        // so the layout can never squeeze the two axes independently.
        const int correctH = qRound(width() / m_aspect);
        if (height() != correctH)
        {
            setFixedHeight(correctH);
        }
        updateGeometry();
    }

    void changeEvent(QEvent* e) override
    {
        QWidget::changeEvent(e);
        if (m_lightRenderer != nullptr && e->type() == QEvent::PaletteChange)
        {
            update();
        }
    }

private:
    // Returns the renderer appropriate for the current palette.
    QSvgRenderer& activeRenderer()
    {
        if (m_lightRenderer != nullptr)
        {
            const QColor windowColor = QGuiApplication::palette().color(QPalette::Window);
            if (windowColor.lightness() >= LIGHT_THEME_LIGHTNESS_THRESHOLD)
            {
                return *m_lightRenderer;
            }
        }
        return m_renderer;
    }


    QSvgRenderer m_renderer;
    std::unique_ptr<QSvgRenderer> m_lightRenderer;
    qreal m_aspect;
    int   m_maxWidth;
};
