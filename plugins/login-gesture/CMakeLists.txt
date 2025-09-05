find_package(DtkWidget REQUIRED)
find_package(DtkCore REQUIRED)

include(GNUInstallDirs)

include_directories(
    ${DFrameworkDBus_INCLUDE_DIRS}
    global
    src
    resetDialog
    utils
)

link_libraries(
    ${Qt_LIBS}
    ${DtkWidget_LIBRARIES}
    ${DtkCore_LIBRARIES}
    crypt
)

set(LIB_NAME login-gesture)
set(BIN_NAME reset-pattern-dialog)

file(GLOB_RECURSE SHARED_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp
)

file(GLOB_RECURSE LIB_SOURCE
        ${CMAKE_CURRENT_SOURCE_DIR}/loginPlugin/*.cpp
)

file(GLOB_RECURSE RESET_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/resetDialog/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resetDialog/*.qrc
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    ADD_DEFINITIONS(-DQM_FILES_DIR="${CMAKE_BINARY_DIR}/plugins/login-gesture/translations")
else()
    ADD_DEFINITIONS(-DQM_FILES_DIR="/usr/share/dde-gesture-login/translations")
endif()

set(QRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/gesture.qrc
)

add_library(${LIB_NAME} SHARED
    ${SHARED_SOURCE}
    ${LIB_SOURCE}
    ${QRCS}
)
set_target_properties(${LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(${BIN_NAME}
    ${SHARED_SOURCE}
    ${RESET_SOURCE}
    ${QRCS}
)

execute_process(COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/translate_generation.sh"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS ${LIB_NAME} LIBRARY DESTINATION lib/dde-session-shell/modules)
install(TARGETS ${BIN_NAME} DESTINATION lib/dde-control-center)

file(GLOB QM_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.qm")
install(FILES ${QM_FILES} DESTINATION share/dde-gesture-login/translations)

install(FILES configs/login-gesture.json DESTINATION /lib/dde-session-shell/modules/config.d)
install(FILES configs/org.deepin.dde.dss-login-gesture.json DESTINATION ${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.lock)
install(FILES configs/org.deepin.dde.dss-login-gesture.json DESTINATION ${CMAKE_INSTALL_DATADIR}/dsg/configs/org.deepin.dde.lightdm-deepin-greeter)