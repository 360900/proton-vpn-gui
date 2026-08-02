cmake_minimum_required(VERSION 3.31)
project(proton_vpn_gui)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Strict conformance, no GNU/MSVC extensions
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS
        Core
        Gui
        Network
        Widgets
        Svg
        DBus
        Quick
        QuickControls2
        Qml
        REQUIRED)

qt_standard_project_setup(REQUIRES 6.8)

find_package(Qt6 COMPONENTS LinguistTools QUIET)

add_subdirectory(core)

qt_add_executable(proton_vpn_gui
        appConfig.h
        appConfig.cpp
        migrations.h
        migrations.cpp
        connectionHistory.h
        connectionHistory.cpp
        favoritesManager.h
        favoritesManager.cpp
        geoUtils.h
        geoUtils.cpp
        dbus/vpnControlAdaptor.h
        dbus/vpnControlAdaptor.cpp
        dbus/vpnStatusAdaptor.h
        dbus/vpnStatusAdaptor.cpp
        resources.qrc
        flags.qrc
        app/main.cpp
        app/imageProviders.h
        app/trayController.h
        app/trayController.cpp
        app/updateChecker.h
        app/updateChecker.cpp
)

set(APP_QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/ConnectionCard.qml
        qml/components/CollapsibleSection.qml
        qml/components/CountryDelegate.qml
        qml/components/CrossfadeStack.qml
        qml/components/FlagIcon.qml
        qml/components/GlobeView.qml
        qml/components/LocationRow.qml
        qml/components/PBadge.qml
        qml/components/PBanner.qml
        qml/components/PButton.qml
        qml/components/PComboBox.qml
        qml/components/PDialog.qml
        qml/components/PIcon.qml
        qml/components/PIconButton.qml
        qml/components/PScrollBar.qml
        qml/components/PSpinner.qml
        qml/components/PSwitch.qml
        qml/components/PTextField.qml
        qml/components/PToast.qml
        qml/components/SearchField.qml
        qml/components/SectionHeader.qml
        qml/components/SettingRow.qml
        qml/components/StarButton.qml
        qml/views/AccountView.qml
        qml/views/LoadingView.qml
        qml/views/LoginView.qml
        qml/views/MainView.qml
        qml/views/NotInstalledView.qml
        qml/views/SettingsView.qml
)

# Flatten paths so every file registers as a root type of the module.
foreach(qml_file ${APP_QML_FILES})
    get_filename_component(qml_alias ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_alias})
endforeach()
set_source_files_properties(qml/theme/Theme.qml PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
        QT_RESOURCE_ALIAS Theme.qml
)

qt_add_qml_module(proton_vpn_gui
        URI ProtonVpnGui
        VERSION 1.0
        QML_FILES ${APP_QML_FILES}
        SOURCES
            app/appSettings.h
            app/appSettings.cpp
            app/geoCoords.h
            app/geoCoords.cpp
            app/serverListModel.h
            app/serverListModel.cpp
            app/themeController.h
            app/themeController.cpp
            app/vpnFacade.h
            app/vpnFacade.cpp
)

# The generated QML type registration includes headers by basename.
target_include_directories(proton_vpn_gui PRIVATE app)

target_link_libraries(proton_vpn_gui PRIVATE
        vpncore
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Widgets
        Qt6::Svg
        Qt6::DBus
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Qml
)

if(Qt6LinguistTools_FOUND)
    qt_add_translations(proton_vpn_gui
            TS_FILES
                i18n/proton_vpn_gui_en.ts
            RESOURCE_PREFIX "/i18n"
            NO_GENERATE_PLURALS_TS_FILE
    )
else()
    message(WARNING "Qt6LinguistTools not found - translations will not be compiled.")
endif()

install(TARGETS proton_vpn_gui
        RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../proton-vpn-gui.desktop
        DESTINATION share/applications
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../proton-vpn-gui.svg
        DESTINATION share/icons/hicolor/scalable/apps
        RENAME proton-vpn-gui.svg
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../io.github._360900.ProtonVpnGui.metainfo.xml
        DESTINATION share/metainfo
)

#  Tests
enable_testing()
add_subdirectory(tests)
