#pragma once

// Shows a dismissable amber warning banner when the app is running
//  as an AppImage, indicating that the AppImage packaging is in beta.

// Note to self for later: to retire, delete this file and remove:
//   - #include "widgets/appImageBetaBanner.h"  (in each page)
//   - m_appImageBetaBanner member declaration   (in each page .h)
//   - checkAppImageBetaBanner() declaration     (in each page .h)
//   - checkAppImageBetaBanner() call            (in each page constructor)
//   - checkAppImageBetaBanner() implementation  (in each page .cpp)

#include "infoBanner.h"
#include "../cli/appImageUtils.h"

class AppImageBetaBanner : public InfoBanner
{
    Q_OBJECT
public:
    explicit AppImageBetaBanner(QWidget* parent = nullptr)
        : InfoBanner(
              tr("The <b>AppImage</b> packaging of this app is currently in <b>beta</b>. "
                 "You may encounter issues that do not occur in the native version. "
                 "Please <a href='https://github.com/wheat32/proton-vpn-qt-app/issues'>"
                 "report any problems on GitHub</a>."),
              parent)
    {}

    // Returns nullptr (and does nothing) when not running as an AppImage,
    // so callers can unconditionally assign the result to a pointer member.
    static AppImageBetaBanner* createIfAppImage(QWidget* parent)
    {
        if (isRunningAsAppImage() == false)
        {
            return nullptr;
        }
        return new AppImageBetaBanner(parent);
    }
};
