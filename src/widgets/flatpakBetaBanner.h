#pragma once

// Shows a dismissable amber warning banner when the app is running
//  as a Flatpak, indicating that the Flatpak packaging is in beta.

// Note to self for later: to retire, delete this file and remove:
//   - #include "widgets/flatpakBetaBanner.h"  (in each page)
//   - m_flatpakBetaBanner member declaration   (in each page .h)
//   - checkFlatpakBetaBanner() declaration     (in each page .h)
//   - checkFlatpakBetaBanner() call            (in each page constructor)
//   - checkFlatpakBetaBanner() implementation  (in each page .cpp)

#include "infoBanner.h"
#include "../cli/flatpakUtils.h"

class FlatpakBetaBanner : public InfoBanner
{
    Q_OBJECT
public:
    explicit FlatpakBetaBanner(QWidget* parent = nullptr)
        : InfoBanner(
              tr("The <b>Flatpak</b> packaging of this app is currently in <b>beta</b>. "
                 "You may encounter issues that do not occur in the native version. "
                 "Please <a href='https://github.com/360900/proton-vpn-gui/issues'>"
                 "report any problems on GitHub</a>."),
              parent)
    {}

    // Returns nullptr (and does nothing) when not running as a Flatpak,
    // so callers can unconditionally assign the result to a pointer member.
    static FlatpakBetaBanner* createIfFlatpak(QWidget* parent)
    {
        if (isRunningAsFlatpak() == false)
        {
            return nullptr;
        }
        return new FlatpakBetaBanner(parent);
    }
};


