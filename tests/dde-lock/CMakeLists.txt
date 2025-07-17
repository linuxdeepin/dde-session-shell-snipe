set(BIN_NAME dde-lock-test)

aux_source_directory(. LOCK_TEST)

set(LOCK_TEST_SRCS
    ${LOCK_TEST}
    ${AUTH_DBUS_SCRS}
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${GLOBAL_UTILS_KEYBOARDMONITOR}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${SWITCHOS_DBUS_SCRS}
    ${AUTHENTICATE}
    ${DBUS_DATA_TYPES}
    ${PLUGIN_MANAGER}
    ${PROJECT_SOURCE_DIR}/src/dde-lock/lockframe.cpp
    ${PROJECT_SOURCE_DIR}/src/dde-lock/lockworker.cpp
    ${PROJECT_SOURCE_DIR}/src/dde-lock/updateworker.cpp
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus/dbuslockagent.cpp
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus/dbuslockfrontservice.cpp
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus/dbusshutdownagent.cpp
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus/dbusshutdownfrontservice.cpp
    ${PROJECT_SOURCE_DIR}/src/global_util/signal_bridge.h
)

add_executable(${BIN_NAME}
    ${LOCK_TEST_SRCS}
    ${QRCS}
    ${LOCK_QRCS}
)

target_include_directories(${BIN_NAME} PUBLIC
    ${DTKWIDGET_INCLUDE_DIR}
    ${DTKCORE_INCLUDE_DIR}
    ${PAM_INCLUDE_DIR}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src/dde-lock
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus
)
if (DISABLE_DSS_SNIPE)
    target_include_directories(${BIN_NAME} PUBLIC
        ${DFrameworkDBus_INCLUDE_DIRS}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${QGSettings_INCLUDE_DIRS}
        ${Qt5X11Extras_INCLUDE_DIRS}
    )
endif ()

target_link_libraries(${BIN_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Dtk${DTK_VERSION_MAJOR}::Widget
    Dtk${DTK_VERSION_MAJOR}::Core
    ${GTEST_LIBRARIES}
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    -lpthread
    -lm
    -lcrypto
)
if (DISABLE_DSS_SNIPE)
    target_link_libraries(${BIN_NAME} PRIVATE
        ${DFrameworkDBus_LIBRARIES}
        ${QGSettings_LIBRARIES}
        ${Qt5X11Extras_LIBRARIES}
        KF5::WaylandClient
    )
endif ()
