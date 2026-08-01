# vpncore - static library holding everything that is not UI:
# process spawning, CLI parsing, VPN state, settings and data models.
# Deliberately links no GUI modules so it stays usable from tests and,
# later, the QML app layer.

qt_add_library(vpncore STATIC
        appImageUtils.h
        cliClient.h
        cliClient.cpp
        cliParsers.h
        cliParsers.cpp
        cliTypes.h
        debug.h
        processRunner.h
        processRunner.cpp
        statusPoller.h
        statusPoller.cpp
        vpnStateMachine.h
        vpnStateMachine.cpp
        fileLogger.h
        fileLogger.cpp
        hostCommand.h
        natPmpService.h
        natPmpService.cpp
        platformUtils.h
        protonCliSettings.h
        protonCliSettings.cpp
        vpnService.h
        vpnService.cpp
)

target_link_libraries(vpncore PUBLIC
        Qt6::Core
)

# Consumers include core headers as "core/xxx.h".
target_include_directories(vpncore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
