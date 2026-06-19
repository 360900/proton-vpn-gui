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
add_qt_test(tst_geoUtils
    tst_geoUtils.cpp
    ../geoUtils.h
    ../geoUtils.cpp
)
target_link_libraries(tst_geoUtils Qt6::Gui Qt6::Svg Qt6::SvgWidgets)

#  AppConfig
add_qt_test(tst_appConfig
    tst_appConfig.cpp
    ../appConfig.h
    ../appConfig.cpp
)

#  ConnectionHistory
add_qt_test(tst_connectionHistory
    tst_connectionHistory.cpp
    ../connectionHistory.h
    ../connectionHistory.cpp
    ../appConfig.h
    ../appConfig.cpp
)

#  StatusMonitor (static parsing helpers only - no subprocess spawned)
add_qt_test(tst_statusMonitor
    tst_statusMonitor.cpp
    ../cli/statusMonitor.h
    ../cli/statusMonitor.cpp
    ../cli/flatpakUtils.h
)

#  UiHelpers
add_qt_test(tst_uiHelpers
    tst_uiHelpers.cpp
    ../uiHelpers.h
)

#  FlatpakUtils
add_qt_test(tst_flatpakUtils
    tst_flatpakUtils.cpp
    ../cli/flatpakUtils.h
)

