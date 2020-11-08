cmake_minimum_required(VERSION 3.13.4)
project(dde-session-shell C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -Wall -pthread -Wl,--as-need -fPIE -Wl,-E")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -march=loongson3a -mhard-float -mno-micromips -mno-mips16 -flax-vector-conversions -mloongson-ext2 -mloongson-mmi")
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

    # generate qm
    execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()

add_subdirectory(unittest)

# Find the library
find_package(PAM REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(DtkWidget REQUIRED)
find_package(DtkCMake REQUIRED)
find_package(Qt5 COMPONENTS
    Core
    Widgets
    Concurrent
    X11Extras
    DBus
    Xml
    Svg
REQUIRED)

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11 xi xcursor xfixes xrandr xext xtst)
pkg_check_modules(DFrameworkDBus REQUIRED dframeworkdbus)
pkg_check_modules(QGSettings REQUIRED gsettings-qt)
pkg_check_modules(Greeter REQUIRED liblightdm-qt5-3)

set(Qt_LIBS
    Qt5::Core
    Qt5::Gui
    Qt5::DBus
    Qt5::Widgets
    Qt5::X11Extras
    Qt5::Xml
    Qt5::Svg
)

function(generation_dbus_interface xml class_name class_file option)
    execute_process(COMMAND qdbusxml2cpp ${option} -p ${class_file} -c ${class_name} ${xml}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction(generation_dbus_interface)

#set(dde_control_center_DBUS_SCRS xml/com.deepin.dde.ControlCenter.xml)
#qt5_add_dbus_interface(dde_control_center_DBUS_SCRS
#xml/com.deepin.dde.ControlCenter.xml
#ControlCenterAdaptor
#)

#set(agent_adapter_DBUS_SRCS xml/com.deepin.daemon.authority.Agent.xml)
#qt5_add_dbus_adaptor(agent_adapter_DBUS_SRCS
#xml/com.deepin.daemon.authority.Agent.xml
#./src/libdde-auth/authagent.h
#AuthAgent
#)

generation_dbus_interface(
    ${CMAKE_SOURCE_DIR}/xml/com.deepin.daemon.Authenticate.xml
    AuthenticateInterface
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface
    -N
)
set(authority_DBUS_SCRS
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface.h
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface.cpp
)

#generation_dbus_interface(
#    ${CMAKE_SOURCE_DIR}/xml/com.deepin.daemon.authority.Transaction.xml
#    TransactionInterface
#    ${CMAKE_CURRENT_BINARY_DIR}/transaction_interface
#    -N
#)
#set(transaction_DBUS_SCRS
#    ${CMAKE_CURRENT_BINARY_DIR}/transaction_interface.h
#    ${CMAKE_CURRENT_BINARY_DIR}/transaction_interface.cpp
#)

generation_dbus_interface(
    ${CMAKE_SOURCE_DIR}/xml/com.huawei.switchos.xml
    HuaWeiSwitchOSInterface
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface
    -N
)
set(haweiswitchos_DBUS_SCRS
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface.h
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface.cpp
)

set(GLOBAL_UTILS_SRCS
    ./src/global_util/util_updateui.h
    ./src/global_util/dbus/dbuslockservice.h
    ./src/global_util/dbus/dbuslockfront.h
    ./src/global_util/dbus/dbuskeyboard.h
    ./src/global_util/dbus/dbusdisplaymanager.h
    ./src/global_util/dbus/displayinterface.h
    ./src/global_util/dbus/dbusmediaplayer2.h
    ./src/global_util/dbus/dbusvariant.h
    ./src/global_util/dbus/dbuslogin1manager.h
    ./src/global_util/dbus/dbuscontrolcenter.h
    ./src/global_util/dbus/dbusinputdevices.h
    ./src/global_util/dbus/dbushotzone.h
    ./src/global_util/multiscreenmanager.h
    ./src/global_util/xkbparser.h
    ./src/global_util/public_func.h
    ./src/global_util/constants.h
    ./src/global_util/keyboardmonitor.h
    ./src/global_util/keyboardplatform.h
    ./src/global_util/keyboardplantform_x11.h
    ./src/global_util/keyboardplantform_wayland.h
    ./src/global_util/xkbparser.cpp
    ./src/global_util/dbus/dbusdisplaymanager.cpp
    ./src/global_util/dbus/dbuslogin1manager.cpp
    ./src/global_util/dbus/dbuscontrolcenter.cpp
    ./src/global_util/dbus/displayinterface.cpp
    ./src/global_util/dbus/dbusvariant.cpp
    ./src/global_util/dbus/dbusmediaplayer2.cpp
    ./src/global_util/dbus/dbuslockfront.cpp
    ./src/global_util/dbus/dbuskeyboard.cpp
    ./src/global_util/dbus/dbusinputdevices.cpp
    ./src/global_util/dbus/dbushotzone.cpp
    ./src/global_util/dbus/dbuslockservice.cpp
    ./src/global_util/util_updateui.cpp
    ./src/global_util/multiscreenmanager.cpp
    ./src/global_util/public_func.cpp
    ./src/global_util/keyboardmonitor.cpp
    ./src/global_util/keyboardplantform_x11.cpp
    ./src/global_util/keyboardplantform_wayland.cpp
)

set(WIDGETS_SRCS
    ./src/widgets/useravatar.h
    ./src/widgets/loginbutton.h
    ./src/widgets/fullscreenbackground.h
    ./src/widgets/userbutton.h
    ./src/widgets/controlwidget.h
    ./src/widgets/shutdownwidget.h
    ./src/widgets/rounditembutton.h
    ./src/widgets/kblayoutwidget.h
    ./src/widgets/mediawidget.h
    ./src/widgets/propertygroup.h
    ./src/widgets/virtualkbinstance.h
    ./src/widgets/logowidget.h
    ./src/widgets/timewidget.h
    ./src/widgets/dlineeditex.h
    ./src/widgets/dpasswordeditex.h
    ./src/widgets/fullscreenbackground.cpp
    ./src/widgets/loginbutton.cpp
    ./src/widgets/mediawidget.cpp
    ./src/widgets/controlwidget.cpp
    ./src/widgets/virtualkbinstance.cpp
    ./src/widgets/propertygroup.cpp
    ./src/widgets/rounditembutton.cpp
    ./src/widgets/useravatar.cpp
    ./src/widgets/userbutton.cpp
    ./src/widgets/shutdownwidget.cpp
    ./src/widgets/kblayoutwidget.cpp
    ./src/widgets/logowidget.cpp
    ./src/widgets/timewidget.cpp
    ./src/widgets/dpasswordeditex.cpp
    ./src/widgets/dlineeditex.cpp
)

set(SESSION_WIDGETS_SRCS
    ./src/session-widgets/userframe.h
    ./src/session-widgets/sessionbasemodel.h
    ./src/session-widgets/authinterface.h
    ./src/session-widgets/lockpasswordwidget.h
    ./src/session-widgets/framedatabind.h
    ./src/session-widgets/sessionbasewindow.h
    ./src/session-widgets/lockcontent.h
    ./src/session-widgets/userinfo.h
    ./src/session-widgets/userloginwidget.h
    ./src/session-widgets/userexpiredwidget.h
    ./src/session-widgets/userframelist.h
    ./src/session-widgets/userlogininfo.h
    ./src/session-widgets/hibernatewidget.h
    ./src/session-widgets/lockpasswordwidget.cpp
    ./src/session-widgets/authinterface.cpp
    ./src/session-widgets/userinfo.cpp
    ./src/session-widgets/framedatabind.cpp
    ./src/session-widgets/sessionbasewindow.cpp
    ./src/session-widgets/userframe.cpp
    ./src/session-widgets/lockcontent.cpp
    ./src/session-widgets/sessionbasemodel.cpp
    ./src/session-widgets/userloginwidget.cpp
    ./src/session-widgets/userexpiredwidget.cpp
    ./src/session-widgets/userframelist.cpp
    ./src/session-widgets/userlogininfo.cpp
    ./src/session-widgets/hibernatewidget.cpp
)

set(LOCK_SRCS
    ${authority_DBUS_SCRS}
    ${transaction_DBUS_SCRS}
    ${GLOBAL_UTILS_SRCS}
    ${WIDGETS_SRCS}
    ${SESSION_WIDGETS_SRCS}
    ./src/libdde-auth/deepinauthframework.cpp
    ./src/libdde-auth/authagent.cpp
    ./src/dde-lock/lockframe.cpp
    ./src/dde-lock/dbus/dbuslockagent.cpp
    ./src/dde-lock/dbus/dbuslockfrontservice.cpp
    ./src/dde-lock/lockworker.cpp
)

set(GREETER_SRCS
    ${authority_DBUS_SCRS}
    ${transaction_DBUS_SCRS}
    ${GLOBAL_UTILS_SRCS}
    ${WIDGETS_SRCS}
    ${SESSION_WIDGETS_SRCS}
    ./src/libdde-auth/deepinauthframework.cpp
    ./src/libdde-auth/authagent.cpp
    ./src/widgets/sessionwidget.h
    ./src/widgets/sessionwidget.cpp
    ./src/lightdm-deepin-greeter/loginwindow.h
    ./src/lightdm-deepin-greeter/greeterworkek.h
    ./src/lightdm-deepin-greeter/loginwindow.cpp
    ./src/lightdm-deepin-greeter/greeterworkek.cpp
    ./src/lightdm-deepin-greeter/logincontent.cpp
)

set(SHUTDOWN_SRCS
    ${haweiswitchos_DBUS_SCRS}
#   ${dde_control_center_DBUS_SCRS}
    ${GLOBAL_UTILS_SRCS}
    ${WIDGETS_SRCS}
    ${SESSION_WIDGETS_SRCS}
    ./src/dde-shutdown/common.h
    ./src/dde-shutdown/app/shutdownframe.h
    ./src/dde-shutdown/view/multiuserswarningview.h
    ./src/dde-shutdown/view/contentwidget.h
    ./src/dde-shutdown/view/systemmonitor.h
    ./src/dde-shutdown/view/warningview.h
    ./src/dde-shutdown/view/inhibitwarnview.h
    ./src/dde-shutdown/shutdownworker.h
    ./src/dde-shutdown/dbusshutdownagent.h
    ./src/dde-shutdown/shutdownworker.cpp
    ./src/dde-shutdown/app/shutdownframe.cpp
    ./src/dde-shutdown/dbusshutdownagent.cpp
    ./src/dde-shutdown/view/systemmonitor.cpp
    ./src/dde-shutdown/view/inhibitwarnview.cpp
    ./src/dde-shutdown/view/multiuserswarningview.cpp
    ./src/dde-shutdown/view/contentwidget.cpp
)

set(QRCS
    ./src/widgets/widgetsimages.qrc
    ./src/widgets/widgetstheme.qrc
)

add_executable(
    dde-lock
    ${LOCK_SRCS}
    ./src/app/dde-lock.cpp
    ${QRCS}
    ./src/dde-lock/images.qrc
    ./src/dde-lock/logintheme.qrc
)
target_include_directories(dde-lock PUBLIC
    ${PAM_INCLUDE_DIR}
    ${DtkWidget_INCLUDE_DIRS}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${QGSettings_INCLUDE_DIRS}
    ${Qt5X11Extras_INCLUDE_DIRS}
)
target_link_libraries(dde-lock PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${QGSettings_LIBRARIES}
)

add_executable(
    dde-shutdown
    ${SHUTDOWN_SRCS}
    ./src/app/dde-shutdown.cpp
    ${QRCS}
    ./src/dde-shutdown/dde-shutdown.pro
    ./src/dde-shutdown/image.qrc
    ./src/dde-shutdown/qss.qrc
)
target_include_directories(dde-shutdown PUBLIC
    ${DtkWidget_INCLUDE_DIRS}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${QGSettings_INCLUDE_DIRS}
    ${Qt5X11Extras_INCLUDE_DIRS}
)
target_link_libraries(dde-shutdown PRIVATE
    ${Qt_LIBS}
    ${XCB_EWMH_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${QGSettings_LIBRARIES}
)

add_executable(
    lightdm-deepin-greeter
    ${GREETER_SRCS}
    ./src/app/lightdm-deepin-greeter.cpp
    ${QRCS}
    ./src/lightdm-deepin-greeter/logintheme.qrc
    ./src/lightdm-deepin-greeter/image.qrc
)
target_include_directories(lightdm-deepin-greeter PUBLIC
    ${PAM_INCLUDE_DIR}
    ${DtkWidget_INCLUDE_DIRS}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${QGSettings_INCLUDE_DIRS}
    ${Qt5X11Extras_INCLUDE_DIRS}
    ${Greeter_INCLUDE_DIRS}
)
target_compile_definitions(lightdm-deepin-greeter
    PRIVATE ENABLE_SESSION
)
target_link_libraries(lightdm-deepin-greeter PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${QGSettings_LIBRARIES}
    ${Greeter_LIBRARIES}
)

# Install settings
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

# bin
install(TARGETS dde-lock dde-shutdown lightdm-deepin-greeter DESTINATION bin)
install(FILES files/deepin-greeter DESTINATION bin)

file(GLOB SCRIPTS  scripts/*)
install(PROGRAMS ${SCRIPTS} DESTINATION /etc/deepin/greeters.d)

file(GLOB QM_FILES "translations/*.qm")
install(FILES ${QM_FILES}
        DESTINATION share/dde-session-shell/translations)

install(FILES files/lightdm-deepin-greeter.desktop DESTINATION share/xgreeters/)
install(FILES files/dde-lock.desktop DESTINATION share/applications/)
install(FILES files/dde-shutdown.desktop DESTINATION share/applications)
install(FILES files/dde-lock-wapper DESTINATION bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES files/dde-shutdown-wapper DESTINATION bin PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES files/dde-session-shell.conf DESTINATION share/dde-session-shell/)

# services
file(GLOB SERVICES files/*.service)
install(FILES ${SERVICES} DESTINATION share/dbus-1/services)
