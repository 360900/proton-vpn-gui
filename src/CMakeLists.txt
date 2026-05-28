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
        appconfig.h
        appconfig.cpp
        thememanager.h
        thememanager.cpp
        connectionhistory.h
        connectionhistory.cpp
        favoritesmanager.h
        favoritesmanager.cpp
        geoutils.h
        geoutils.cpp
        uihelpers.h
        vpnmanager.h
        vpnmanager.cpp
        dbus/vpnstatusadaptor.h
        dbus/vpnstatusadaptor.cpp
        cli/protonvpncli.cpp
        cli/natpmpmanager.h
        cli/natpmpmanager.cpp
        cli/statusmonitor.h
        cli/statusmonitor.cpp
        cli/flatpakutils.h
        mainwindow.h
        mainwindow.cpp
        widgets/elidalabel.h
        widgets/infobanner.h
        widgets/flatpakbetabanner.h
        widgets/numberspinner.h
        widgets/pickerbase.h
        widgets/pickerdrawer.h
        widgets/pickerdrawer.cpp
        widgets/svgbanner.h
        widgets/toastnotification.h
        widgets/toggleswitch.h
        widgets/toggleswitch.cpp
        widgets/togglewithstatus.h
        pages/notinstalledpage.h
        pages/notinstalledpage.cpp
        pages/loginpage.h
        pages/loginpage.cpp
        pages/vpnpage.h
        pages/vpnpage.cpp
        pages/countriespage.h
        pages/countriespage.cpp
        pages/accountpage.h
        pages/accountpage.cpp
        pages/settingspage.h
        pages/settingspage.cpp
        pages/debugpage.h
        pages/debugpage.cpp
        dialogs/aboutdialog.h
        dialogs/aboutdialog.cpp
        dialogs/errordetailsdialog.h
        dialogs/errordetailsdialog.cpp
        dialogs/whatsnewdialog.h
        dialogs/whatsnewdialog.cpp
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
    message(WARNING "Qt6LinguistTools not found — translations will not be compiled.")
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

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)



