#include "thememanager.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QGuiApplication>
#include <QStyleHints>

namespace ThemeManager
{

static void applyDark()
{
    QPalette palette;
    constexpr QColor bg(0x1a, 0x1a, 0x2e);
    constexpr QColor surface(0x25, 0x25, 0x3d);
    constexpr QColor border(0x3a, 0x3a, 0x55);
    constexpr QColor accent(0x6d, 0x4a, 0xff);
    constexpr QColor textPrimary(0xea, 0xea, 0xea);
    constexpr QColor textSecondary(0x99, 0x99, 0xbb);

    palette.setColor(QPalette::Window,          bg);
    palette.setColor(QPalette::WindowText,      textPrimary);
    palette.setColor(QPalette::Base,            surface);
    palette.setColor(QPalette::AlternateBase,   bg);
    palette.setColor(QPalette::ToolTipBase,     surface);
    palette.setColor(QPalette::ToolTipText,     textPrimary);
    palette.setColor(QPalette::Text,            textPrimary);
    palette.setColor(QPalette::BrightText,      Qt::white);
    palette.setColor(QPalette::Button,          surface);
    palette.setColor(QPalette::ButtonText,      textPrimary);
    palette.setColor(QPalette::Link,            accent);
    palette.setColor(QPalette::Highlight,       accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, textSecondary);
    palette.setColor(QPalette::Mid,             border);
    palette.setColor(QPalette::Dark,            border);
    palette.setColor(QPalette::Midlight,        surface);
    palette.setColor(QPalette::Shadow,          QColor(0x00, 0x00, 0x00, 0x80));
    QApplication::setPalette(palette);

    QFile f(QStringLiteral(":/style.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
}

static void applyLight()
{
    QPalette palette;
    constexpr QColor bg(0xf0, 0xf0, 0xf5);
    constexpr QColor surface(0xff, 0xff, 0xff);
    constexpr QColor border(0xc8, 0xc8, 0xd8);
    constexpr QColor accent(0x6d, 0x4a, 0xff);
    constexpr QColor textPrimary(0x1a, 0x1a, 0x2e);
    constexpr QColor textSecondary(0x5a, 0x5a, 0x7a);

    palette.setColor(QPalette::Window,          bg);
    palette.setColor(QPalette::WindowText,      textPrimary);
    palette.setColor(QPalette::Base,            surface);
    palette.setColor(QPalette::AlternateBase,   bg);
    palette.setColor(QPalette::ToolTipBase,     surface);
    palette.setColor(QPalette::ToolTipText,     textPrimary);
    palette.setColor(QPalette::Text,            textPrimary);
    palette.setColor(QPalette::BrightText,      Qt::black);
    palette.setColor(QPalette::Button,          surface);
    palette.setColor(QPalette::ButtonText,      textPrimary);
    palette.setColor(QPalette::Link,            accent);
    palette.setColor(QPalette::Highlight,       accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, textSecondary);
    palette.setColor(QPalette::Mid,             border);
    palette.setColor(QPalette::Dark,            border);
    palette.setColor(QPalette::Midlight,        QColor(0xe8, 0xe8, 0xf4));
    palette.setColor(QPalette::Shadow,          QColor(0x00, 0x00, 0x00, 0x30));
    QApplication::setPalette(palette);

    QFile f(QStringLiteral(":/style_light.qss"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
}

// Detect whether the system is using a dark colour scheme.
static bool systemIsDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    // Fall back: check the system palette window background luminance.
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
}

void apply(AppConfig::Theme theme)
{
    bool useDark = false;
    switch (theme)
    {
    case AppConfig::Theme::Dark:
        useDark = true;
        break;
    case AppConfig::Theme::Light:
        useDark = false;
        break;
    default:
        useDark = systemIsDark();
        break;
    }

    if (useDark)
    {
        applyDark();
    }
    else
    {
        applyLight();
    }
}

} // namespace ThemeManager
