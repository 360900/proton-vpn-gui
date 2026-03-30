cmake_minimum_required(VERSION 3.31)
project(proton_vpn_qt)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Widgets
        Svg
        SvgWidgets
        REQUIRED)

set(SOURCES
        main.cpp
        appconfig.h
        appconfig.cpp
        connectionhistory.h
        connectionhistory.cpp
        geoutils.h
        geoutils.cpp
        uihelpers.h
        vpnmanager.h
        vpnmanager.cpp
        mainwindow.h
        mainwindow.cpp
        widgets/elidalabel.h
        widgets/infobanner.h
        widgets/numberspinner.h
        widgets/pickerbase.h
        widgets/svgbanner.h
        widgets/toastnotification.h
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
        resources.qrc
        flags.qrc
)

add_executable(proton_vpn_qt ${SOURCES})

target_link_libraries(proton_vpn_qt
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        Qt6::SvgWidgets
)



