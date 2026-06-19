cmake_minimum_required(VERSION 3.31)
project(proton_vpn_qt)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Strict conformance, no GNU/MSVC extensions
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Svg
        SvgWidgets
        DBus
        REQUIRED)

find_package(Qt6 COMPONENTS LinguistTools QUIET)

set(SOURCES
        main.cpp
        appConfig.h
        appConfig.cpp
        migrations.h
        migrations.cpp
        themeManager.h
        themeManager.cpp
        connectionHistory.h
        connectionHistory.cpp
        favoritesManager.h
        favoritesManager.cpp
        geoUtils.h
        geoUtils.cpp
        uiHelpers.h
        vpnManager.h
        vpnManager.cpp
        dbus/vpnStatusAdaptor.h
        dbus/vpnStatusAdaptor.cpp
        cli/protonvpnCli.cpp
        cli/natpmpManager.h
        cli/natpmpManager.cpp
        cli/statusMonitor.h
        cli/statusMonitor.cpp
        cli/flatpakUtils.h
        mainWindow.h
        mainWindow.cpp
        widgets/elidaLabel.h
        widgets/infoBanner.h
        widgets/flatpakBetaBanner.h
        widgets/numberSpinner.h
        widgets/pickerBase.h
        widgets/pickerDrawer.h
        widgets/pickerDrawer.cpp
        widgets/svgBanner.h
        widgets/toastNotification.h
        widgets/toggleSwitch.h
        widgets/toggleSwitch.cpp
        widgets/toggleWithStatus.h
        pages/notInstalledPage.h
        pages/notInstalledPage.cpp
        pages/loginPage.h
        pages/loginPage.cpp
        pages/vpnPage.h
        pages/vpnPage.cpp
        pages/countriesPage.h
        pages/countriesPage.cpp
        pages/accountPage.h
        pages/accountPage.cpp
        pages/settingsPage.h
        pages/settingsPage.cpp
        pages/debugPage.h
        pages/debugPage.cpp
        dialogs/aboutDialog.h
        dialogs/aboutDialog.cpp
        dialogs/errorDetailsDialog.h
        dialogs/errorDetailsDialog.cpp
        dialogs/whatsNewDialog.h
        dialogs/whatsNewDialog.cpp
        resources.qrc
        flags.qrc
)

add_executable(proton_vpn_qt ${SOURCES})

if(Qt6LinguistTools_FOUND)
    qt_add_translations(proton_vpn_qt
            TS_FILES
                i18n/proton_vpn_qt_en.ts
            RESOURCE_PREFIX "/i18n"
            NO_GENERATE_PLURALS_TS_FILE
    )
else()
    message(WARNING "Qt6LinguistTools not found - translations will not be compiled.")
endif()

target_link_libraries(proton_vpn_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        Qt6::SvgWidgets
        Qt6::DBus
)

install(TARGETS proton_vpn_qt
        RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../proton-vpn-qt-app.desktop
        DESTINATION share/applications
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../proton-vpn-sign.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME proton-vpn-qt-app.svg
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../io.github.wheat32.ProtonVPNQt.metainfo.xml
        DESTINATION share/metainfo
)

#  Tests
enable_testing()
add_subdirectory(tests)



