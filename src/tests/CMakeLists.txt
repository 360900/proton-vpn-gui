cmake_minimum_required(VERSION 3.31)

enable_testing()

find_package(Qt6 COMPONENTS Test REQUIRED)

#  Helper macro
# add_qt_test(name sources...)
# Creates an executable, links Qt6::Test + Qt6::Core, and registers it with
# CTest so `ctest` (or cmake --build . --target test) picks it up.
macro(add_qt_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(${name}
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME ${name} COMMAND ${name})
endmacro()

#  GeoUtils
add_qt_test(tst_geoutils
    tst_geoutils.cpp
    ../geoutils.h
    ../geoutils.cpp
)
target_link_libraries(tst_geoutils Qt6::Gui Qt6::Svg Qt6::SvgWidgets)

#  AppConfig
add_qt_test(tst_appconfig
    tst_appconfig.cpp
    ../appconfig.h
    ../appconfig.cpp
)

#  ConnectionHistory
add_qt_test(tst_connectionhistory
    tst_connectionhistory.cpp
    ../connectionhistory.h
    ../connectionhistory.cpp
    ../appconfig.h
    ../appconfig.cpp
)

#  StatusMonitor (static parsing helpers only — no subprocess spawned)
add_qt_test(tst_statusmonitor
    tst_statusmonitor.cpp
    ../cli/statusmonitor.h
    ../cli/statusmonitor.cpp
    ../cli/flatpakutils.h
)

#  UiHelpers
add_qt_test(tst_uihelpers
    tst_uihelpers.cpp
    ../uihelpers.h
)

#  FlatpakUtils
add_qt_test(tst_flatpakutils
    tst_flatpakutils.cpp
    ../cli/flatpakutils.h
)

