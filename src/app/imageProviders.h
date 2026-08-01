#pragma once
// imageProviders.h
// QQuickImageProviders for flags and tinted monochrome icons. Both render
// SVGs at requestedSize (which QML sets from sourceSize * devicePixelRatio),
// so icons are crisp at any display scale - the single tinting strategy that
// replaces the three the widgets UI used.

#include <QColor>
#include <QPainter>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QSvgRenderer>

// "image://flag/<cc>" -> :/flags/<cc> (4:3 SVGs, no extension in the qrc).
class FlagImageProvider final : public QQuickImageProvider
{
public:
    FlagImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const QSize target = requestedSize.isValid() ? requestedSize : QSize(24, 18);
        QSvgRenderer renderer(QStringLiteral(":/flags/") + id.toLower());
        if (renderer.isValid() == false)
        {
            if (size != nullptr)
            {
                *size = QSize();
            }
            return {};
        }
        QPixmap pix(target);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        renderer.render(&painter);
        painter.end();
        if (size != nullptr)
        {
            *size = target;
        }
        return pix;
    }
};

// "image://icon/<name>?<color>" -> :/assets/<name>.svg tinted with <color>.
class IconImageProvider final : public QQuickImageProvider
{
public:
    IconImageProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const int queryPos = id.lastIndexOf(QLatin1Char('?'));
        const QString name = queryPos >= 0 ? id.left(queryPos) : id;
        const QColor tint  = queryPos >= 0 ? QColor(id.mid(queryPos + 1)) : QColor();

        const QSize target = requestedSize.isValid() ? requestedSize : QSize(18, 18);
        QSvgRenderer renderer(QStringLiteral(":/assets/") + name + QStringLiteral(".svg"));
        if (renderer.isValid() == false)
        {
            if (size != nullptr)
            {
                *size = QSize();
            }
            return {};
        }
        QPixmap pix(target);
        pix.fill(Qt::transparent);
        QPainter painter(&pix);
        renderer.render(&painter);
        if (tint.isValid())
        {
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pix.rect(), tint);
        }
        painter.end();
        if (size != nullptr)
        {
            *size = target;
        }
        return pix;
    }
};
