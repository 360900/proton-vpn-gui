#pragma once
#include "appConfig.h"

// ---------------------------------------------------------------------------
// ThemeManager – applies a palette + stylesheet to the running QApplication.
// Call apply() at startup and whenever the user changes the theme setting.
// ---------------------------------------------------------------------------
namespace ThemeManager
{
    // Resolves Theme::System to Dark or Light by inspecting the system palette,
    // then applies the appropriate palette and QSS stylesheet.
    void apply(AppConfig::Theme theme);
}

