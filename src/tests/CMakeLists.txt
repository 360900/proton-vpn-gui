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
        vpncore
        Qt6::Core
        Qt6::Test
    )
    add_test(NAME ${name} COMMAND ${name})
endmacro()

#  CliParsers (fixture-driven - parses captured real CLI output)
add_qt_test(tst_cliParsers
    tst_cliParsers.cpp
)
target_compile_definitions(tst_cliParsers PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/cli"
)

#  CliClient (via FakeProcessRunner - nothing is spawned)
add_qt_test(tst_cliClient
    tst_cliClient.cpp
    support/fakeProcessRunner.h
)

#  ProcessRunner (integration - spawns tiny real processes: echo/sh/sleep/cat)
add_qt_test(tst_processRunner
    tst_processRunner.cpp
)

#  VpnStateMachine (watchdog runs with ms intervals)
add_qt_test(tst_vpnStateMachine
    tst_vpnStateMachine.cpp
)

#  StatusPoller (via FakeProcessRunner)
add_qt_test(tst_statusPoller
    tst_statusPoller.cpp
    support/fakeProcessRunner.h
)

#  NatPmpService (via FakeProcessRunner)
add_qt_test(tst_natPmpService
    tst_natPmpService.cpp
    support/fakeProcessRunner.h
)

#  VpnService (orchestration over FakeProcessRunner)
add_qt_test(tst_vpnService
    tst_vpnService.cpp
    support/fakeProcessRunner.h
)

#  GeoUtils
add_qt_test(tst_geoUtils
    tst_geoUtils.cpp
    ../geoUtils.h
    ../geoUtils.cpp
)
target_link_libraries(tst_geoUtils Qt6::Gui Qt6::Svg)

#  GeoCoords (country centroid lookup for the globe animation)
add_qt_test(tst_geoCoords
    tst_geoCoords.cpp
    ../app/geoCoords.h
    ../app/geoCoords.cpp
    ../geoUtils.h
    ../geoUtils.cpp
)
target_link_libraries(tst_geoCoords Qt6::Gui Qt6::Svg Qt6::Qml)

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

#  FlatpakUtils
add_qt_test(tst_flatpakUtils
    tst_flatpakUtils.cpp
    ../core/hostCommand.h
    ../core/appImageUtils.h
)

