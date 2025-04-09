set(BIN_NAME lightdm-deepin-greeter-test)

aux_source_directory(. GREETER_TEST)

set(GREETER_TEST_SRCS
    ${GREETER_TEST}
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
    ${UPDATE}
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/loginwindow.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/greeterworker.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/logincontent.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/logintipswindow.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/sessionwidget.cpp
)

add_executable(${BIN_NAME}
    ${GREETER_TEST_SRCS}
    ${QRCS}
    ${GREETER_QRCS}
)

target_include_directories(${BIN_NAME} PUBLIC
    ${PAM_INCLUDE_DIR}
    ${DTKWIDGET_INCLUDE_DIR}
    ${DTKCORE_INCLUDE_DIR}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${Greeter_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter
)
if (DISABLE_DSS_SNIPE)
    target_include_directories(${BIN_NAME} PUBLIC
        ${DFrameworkDBus_INCLUDE_DIRS}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${QGSettings_INCLUDE_DIRS}
        ${Qt5X11Extras_INCLUDE_DIRS}
    )
endif ()

target_compile_definitions(lightdm-deepin-greeter
    PRIVATE ENABLE_SESSION
)

target_link_libraries(${BIN_NAME} PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    Dtk${DTK_VERSION_MAJOR}::Widget
    Dtk${DTK_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
    ${Greeter_LIBRARIES}
    ${GTEST_LIBRARIES}
    -lpthread
    -lm
    -lcrypto
)
if (DISABLE_DSS_SNIPE)
    target_link_libraries(${BIN_NAME} PRIVATE
        ${DFrameworkDBus_LIBRARIES}
        ${Qt5Gui_PRIVATE_LIBRARIES}
        ${QGSettings_LIBRARIES}
        ${Qt5X11Extras_LIBRARIES}
        KF5::WaylandClient
    )
endif ()
