set(BIN_NAME lightdm-deepin-greeter-test)

aux_source_directory(. GREETER_TEST)

set(GREETER_TEST_SRCS
    ${GREETER_TEST}
    ${authority_DBUS_SCRS}
    ${transaction_DBUS_SCRS}
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${GLOBAL_UTILS_KEYBOARDMONITOR}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${haweiswitchos_DBUS_SCRS}
    ${AUTHENTICATE}
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/loginwindow.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/greeterworker.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/logincontent.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/logintipswindow.cpp
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/sessionwidget.cpp
)
if (USE_DEEPIN_WAYLAND)
  set(GREETER_TEST_SRCS
    ${GREETER_TEST_SRCS}
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter/greeter_display_wayland.cpp
  )
endif(USE_DEEPIN_WAYLAND)

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
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${QGSettings_INCLUDE_DIRS}
    ${Qt5X11Extras_INCLUDE_DIRS}
    ${Greeter_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter
)

target_compile_definitions(lightdm-deepin-greeter
    PRIVATE ENABLE_SESSION
)

target_link_libraries(${BIN_NAME} PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${DtkCore_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${QGSettings_LIBRARIES}
    ${Greeter_LIBRARIES}
    ${Qt5Test_LIBRARIES}
    ${GTEST_LIBRARIES}
    KF5::WaylandClient
    -lpthread
    -lm
    -lcrypto
)
