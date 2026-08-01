// themeController.cpp
// See themeController.h.

#include "themeController.h"

#include "../appConfig.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace
{
ThemeController* s_instance = nullptr;
} // namespace

ThemeController* ThemeController::instance()
{
    if (s_instance == nullptr)
    {
        s_instance = new ThemeController();
    }
    return s_instance;
}

ThemeController* ThemeController::create(QQmlEngine*, QJSEngine*)
{
    ThemeController* inst = instance();
    QQmlEngine::setObjectOwnership(inst, QQmlEngine::CppOwnership);
    return inst;
}

ThemeController::ThemeController(QObject* parent)
    : QObject(parent)
{
    m_mode = static_cast<Mode>(AppConfig::instance().theme());
    resolveDark();

    // Follow the desktop theme live while in System mode.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this](Qt::ColorScheme)
            {
                if (m_mode == Mode::System)
                {
                    resolveDark();
                }
            });
}

void ThemeController::setMode(const Mode mode)
{
    if (m_mode == mode)
    {
        return;
    }
    m_mode = mode;
    AppConfig::instance().setTheme(static_cast<AppConfig::Theme>(mode));
    emit modeChanged();
    resolveDark();
}

bool ThemeController::systemPrefersDark() const
{
    // Default to dark when the desktop expresses no preference - dark is the
    // Proton house style.
    return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
}

void ThemeController::resolveDark()
{
    const bool dark = m_mode == Mode::Dark ||
                      (m_mode == Mode::System && systemPrefersDark());
    if (m_dark != dark)
    {
        m_dark = dark;
        emit darkChanged();
    }
}
