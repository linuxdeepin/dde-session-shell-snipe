cmake_minimum_required(VERSION 3.17)
project(dde-session-shell C CXX)

include(GNUInstallDirs)
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -Wall")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# coverage option
# cmake -DENABLE_COVERAGE=ON ..
OPTION (ENABLE_COVERAGE "Use gcov" OFF)
MESSAGE(STATUS ENABLE_COVERAGE=${ENABLE_COVERAGE})
if (ENABLE_COVERAGE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    #SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -march=loongson3a -mhard-float -mno-micromips -mno-mips16 -flax-vector-conversions -mloongson-ext2 -mloongson-mmi")
endif()

# DDE_SESSION_SHELL_SNIPE defined in snipe repo
if (NOT DDE_SESSION_SHELL_SNIPE)
    find_package(Qt6 COMPONENTS Core QUIET)
endif ()

if (DDE_SESSION_SHELL_SNIPE OR Qt6_FOUND)
    add_definitions(-DENABLE_DSS_SNIPE)
    set(DISABLE_DSS_SNIPE FALSE)
    set(QT_VERSION_MAJOR 6)
    set(DTK_VERSION_MAJOR 6)
else ()
    # for v20
    set(DISABLE_DSS_SNIPE TRUE)
    set(QT_VERSION_MAJOR 5)
    set(DTK_VERSION_MAJOR "")
endif ()

message(STATUS "Using Qt${QT_VERSION_MAJOR} (Dtk${DTK_VERSION_MAJOR})")

# Find the library
find_package(PAM REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets DBus Svg Network)
find_package(Dtk${DTK_VERSION_MAJOR} REQUIRED COMPONENTS Widget Core)

if (DISABLE_DSS_SNIPE)
    find_package(KF5Wayland REQUIRED)
    find_package(Qt5 COMPONENTS X11Extras REQUIRED)

    pkg_check_modules(DFrameworkDBus REQUIRED dframeworkdbus)
    pkg_check_modules(QGSettings REQUIRED gsettings-qt)
    pkg_check_modules(Greeter REQUIRED liblightdm-qt5-3)
else ()
    find_package(Qt6 COMPONENTS LinguistTools REQUIRED)
    find_package(Dtk${DTK_VERSION_MAJOR} COMPONENTS REQUIRED Tools)
    pkg_check_modules(Greeter REQUIRED liblightdm-qt6-3)
endif ()

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11 xi xcursor xfixes xrandr xext xtst)
pkg_check_modules(SSL REQUIRED IMPORTED_TARGET libcrypto libssl openssl)

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    if (DISABLE_DSS_SNIPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")
        # generate qm
        execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    else ()
        file(GLOB TS_FILES "translations/*.ts")
        qt_add_translation(QM_FILES ${TS_FILES})
        add_custom_target(dde-session-shell_language ALL DEPENDS ${QM_FILES})
    endif ()
endif ()

set(Qt_LIBS
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
)

if (DISABLE_DSS_SNIPE)
    list(APPEND Qt_LIBS Qt5::X11Extras)
endif()

if (DISABLE_DSS_SNIPE)
    try_compile(USE_DEEPIN_WAYLAND
          ${CMAKE_CURRENT_BINARY_DIR}/try_compile
        SOURCES
          ${CMAKE_CURRENT_SOURCE_DIR}/tests/wayland_test.cc
        LINK_LIBRARIES
          ${Qt_LIBS}
          ${Qt5Widgets_LIBRARIES}
          KF5::WaylandClient
    )

    message(STATUS "USE_DEEPIN_WAYLAND ${USE_DEEPIN_WAYLAND}")

    if(USE_DEEPIN_WAYLAND)
      add_definitions(-DUSE_DEEPIN_WAYLAND)
    endif()
endif ()

try_compile(USE_GM
        ${CMAKE_CURRENT_BINARY_DIR}/try_compile
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gm.cc
    LINK_LIBRARIES
        PkgConfig::SSL
    OUTPUT_VARIABLE
        GM_MESSAGE
)

message(STATUS "check GM: ${USE_GM}")

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    message(${GM_MESSAGE})
endif()

if(USE_GM)
  # 优先使用国密
  add_definitions(-DPREFER_USING_GM)
endif()

function(generation_dbus_interface xml class_name class_file option)
    execute_process(COMMAND /usr/lib/qt${QT_VERSION_MAJOR}/bin/qdbusxml2cpp ${option} -p ${class_file} -c ${class_name} ${xml}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction(generation_dbus_interface)

set (SWITCH_OS_XML "${CMAKE_SOURCE_DIR}/xml/com.huawei.switchos.xml")
if (DISABLE_DSS_SNIPE)
    set(AUTHENTICATE_XML "${CMAKE_SOURCE_DIR}/xml/com.deepin.daemon.Authenticate.xml")
else ()
    set(AUTHENTICATE_XML "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.Authenticate1.xml")
endif ()

generation_dbus_interface(
    ${AUTHENTICATE_XML}
    AuthenticateInterface
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface
    -N
)
set(AUTH_DBUS_SCRS
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface.h
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface.cpp
)

generation_dbus_interface(
    ${SWITCH_OS_XML}
    HuaWeiSwitchOSInterface
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface
    -N
)
set(SWITCHOS_DBUS_SCRS
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface.h
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface.cpp
)

# for snipe
if (NOT DISABLE_DSS_SNIPE)
    set(XML2CPP_DIR ${PROJECT_SOURCE_DIR}/src/global_util/dbus)
    function(generation_snipe_dbus_interface xml class_file)
        set(file_name ${XML2CPP_DIR}/${class_file})
        execute_process(
                COMMAND ${DTK_XML2CPP} -p ${file_name} ${xml}
                WORKING_DIRECTORY ${XML2CPP_DIR}
        )
    endfunction()

    set(DBUS_INTERFACES
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.Authenticate1.xml authenticate1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.Accounts1.xml accounts1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.Accounts1.User.xml userinterface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.ImageEffect1.xml imageeffect1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.Logined.xml loginedinterface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.PowerManager1.xml powermanager1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.SystemPower1.xml systempower1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/com.deepin.wm.xml wminterface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.Authenticate1.Session2.xml session2interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.SessionManager1.xml sessionmanager1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.deepin.dde.SystemDisplay1.xml systemdisplay1interface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.freedesktop.DBus.xml dbusinterface"
        "${CMAKE_SOURCE_DIR}/xml/snipe/org.freedesktop.login1.session.self.xml selfinterface"
    )

    foreach(interface IN LISTS DBUS_INTERFACES)
        string(REPLACE " " ";" interface_list "${interface}")

        list(GET interface_list 0 XML_FILE)
        list(GET interface_list 1 CLASS_FILE)

        list(LENGTH interface_list list_len)
        if(list_len LESS 2)
            message(FATAL_ERROR "Invalid DBus interface definition: ${interface}")
        endif()

        generation_snipe_dbus_interface(${XML_FILE} ${CLASS_FILE})
    endforeach()
endif ()

include_directories(${PROJECT_SOURCE_DIR}/src/global_util)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/dbus)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/keyboardmonitor)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/plugin_manager)
include_directories(${PROJECT_SOURCE_DIR}/src/widgets)
include_directories(${PROJECT_SOURCE_DIR}/src/session-widgets)
include_directories(${PROJECT_SOURCE_DIR}/src/libdde-auth)
include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/dbus/types)

aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util GLOBAL_UTILS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/dbus GLOBAL_UTILS_DBUS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/widgets WIDGETS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/session-widgets SESSION_WIDGETS)
## 需要把libdde-auth中的.h编译出moc文件，故采用这种方式
file(GLOB AUTHENTICATE ${PROJECT_SOURCE_DIR}/src/libdde-auth/*.h ${PROJECT_SOURCE_DIR}/src/libdde-auth/*.cpp)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/keyboardmonitor GLOBAL_UTILS_KEYBOARDMONITOR)
aux_source_directory(${PROJECT_SOURCE_DIR}/interface INTERFACE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/dbus/types DBUS_DATA_TYPES)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/plugin_manager PLUGIN_MANAGER)

set(QRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${PROJECT_SOURCE_DIR}/src/widgets/widgetsimages.qrc
    ${PROJECT_SOURCE_DIR}/src/widgets/widgetstheme.qrc
)

set(LOCK_SRCS
    ${AUTH_DBUS_SCRS}
    ${SWITCHOS_DBUS_SCRS}
    ${SYSTEMD_DISPLAY1_DBUS_SCRS}
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${GLOBAL_UTILS_KEYBOARDMONITOR}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${AUTHENTICATE}
    ${INTERFACE}
    ${DBUS_DATA_TYPES}
    ${PLUGIN_MANAGER}
    src/app/dde-lock.cpp
    src/dde-lock/lockframe.cpp
    src/dde-lock/lockworker.cpp
    src/dde-lock/updateworker.cpp
    src/dde-lock/dbus/dbuslockagent.cpp
    src/dde-lock/dbus/dbuslockfrontservice.cpp
    src/dde-lock/dbus/dbusshutdownagent.cpp
    src/dde-lock/dbus/dbusshutdownfrontservice.cpp
    src/widgets/passworderrortipswidget.cpp
    src/widgets/passworderrortipswidget.h
    src/global_util/dbusconstant.h
    src/global_util/signal_bridge.h
)

link_libraries(
    Threads::Threads
)

add_executable(dde-lock
    ${LOCK_SRCS}
    ${QRCS}
)

target_include_directories(dde-lock PUBLIC
    ${PAM_INCLUDE_DIR}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/src/dde-lock
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus
)
if (DISABLE_DSS_SNIPE)
    target_include_directories(dde-lock PUBLIC
        ${DTKWIDGET_INCLUDE_DIR}
        ${DTKCORE_INCLUDE_DIR}
        ${DFrameworkDBus_INCLUDE_DIRS}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${QGSettings_INCLUDE_DIRS}
        ${Qt5X11Extras_INCLUDE_DIRS}
    )
endif ()

target_link_libraries(dde-lock PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    Dtk${DTK_VERSION_MAJOR}::Widget
    Dtk${DTK_VERSION_MAJOR}::Core
    ${Greeter_LIBRARIES}
    PkgConfig::SSL
)
if (DISABLE_DSS_SNIPE)
    target_link_libraries(dde-lock PRIVATE
        ${DFrameworkDBus_LIBRARIES}
        ${QGSettings_LIBRARIES}
        KF5::WaylandClient
    )
endif()

set(GREETER_SRCS
    ${AUTH_DBUS_SCRS}
    ${SWITCHOS_DBUS_SCRS}
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${GLOBAL_UTILS_KEYBOARDMONITOR}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${AUTHENTICATE}
    ${INTERFACE}
    ${DBUS_DATA_TYPES}
    ${PLUGIN_MANAGER}
    src/app/lightdm-deepin-greeter.cpp
    src/lightdm-deepin-greeter/loginwindow.cpp
    src/lightdm-deepin-greeter/greeterworker.cpp
    src/lightdm-deepin-greeter/logincontent.cpp
    src/lightdm-deepin-greeter/logintipswindow.cpp
    src/lightdm-deepin-greeter/sessionwidget.cpp
    src/global_util/signal_bridge.h
)

if (USE_DEEPIN_WAYLAND)
  set(GREETER_SRCS
    ${GREETER_SRCS}
  )
endif(USE_DEEPIN_WAYLAND)

add_executable(lightdm-deepin-greeter
    ${GREETER_SRCS}
    ${QRCS}
)

target_include_directories(lightdm-deepin-greeter PUBLIC
    ${PAM_INCLUDE_DIR}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${Greeter_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src/lightdm-deepin-greeter
)
if (DISABLE_DSS_SNIPE)
    target_include_directories(lightdm-deepin-greeter PUBLIC
        ${DTKWIDGET_INCLUDE_DIR}
        ${DTKCORE_INCLUDE_DIR}
        ${DFrameworkDBus_INCLUDE_DIRS}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${QGSettings_INCLUDE_DIRS}
        ${Qt5X11Extras_INCLUDE_DIRS}
    )
endif ()

target_compile_definitions(lightdm-deepin-greeter
    PRIVATE ENABLE_SESSION
)

if (WAIT_DEEPIN_ACCOUNTS_SERVICE)
    target_compile_definitions(lightdm-deepin-greeter
        PUBLIC ENABLE_WAITING_ACCOUNTS_SERVICE
    )
endif()

target_link_libraries(lightdm-deepin-greeter PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    Dtk${DTK_VERSION_MAJOR}::Widget
    Dtk${DTK_VERSION_MAJOR}::Core
    ${Greeter_LIBRARIES}
    PkgConfig::SSL
)

if (DISABLE_DSS_SNIPE)
    target_link_libraries(lightdm-deepin-greeter PRIVATE
        ${DFrameworkDBus_LIBRARIES}
        ${QGSettings_LIBRARIES}
        KF5::WaylandClient
    )
endif()

if (DISABLE_DSS_SNIPE)
    add_executable(greeter-display-setting
        src/app/greeter-display-setting.cpp
    )

    target_link_libraries(greeter-display-setting
        ${Qt_LIBS}
        ${XCB_EWMH_LIBRARIES}
        ${DtkCore_LIBRARIES}
        ${Qt5X11Extras_LIBRARIES}
        ${Qt5DBus_LIBRARIES}
    )
endif ()

if (DISABLE_DSS_SNIPE)
    add_subdirectory(lighter-greeter)
endif ()
add_subdirectory(tests)
add_subdirectory(plugins)
add_subdirectory(src/pam-inhibit-autologin)

if (DISABLE_DSS_SNIPE)
    # bin
    install(TARGETS dde-lock lightdm-deepin-greeter greeter-display-setting DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(PROGRAMS files/deepin-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})

    # install x files
    file(GLOB SCRIPTS
        files/x/00-xrandr
        files/x/10-cursor-theme
        files/x/lightdm-deepin-greeter
    )
    install(PROGRAMS ${SCRIPTS} DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/greeters.d/x)
    # 选择需要执行的二进制
    install(PROGRAMS files/launch-binary DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/greeters.d)

    # install wayland files
    install(PROGRAMS files/wayland/launch-kwin-wayland DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/greeters.d/wayland)
    install(PROGRAMS files/wayland/lightdm-deepin-greeter-wayland DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/greeters.d/wayland)
    install(FILES files/wayland/lightdm-deepin-greeter-wayland.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/xgreeters/)
else () # snipe
    install(TARGETS dde-lock lightdm-deepin-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(PROGRAMS files/snipe/deepin-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})

    file(GLOB SCRIPTS  files/snipe/scripts/*)
    install(PROGRAMS ${SCRIPTS} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/deepin/greeters.d)
endif ()

# 指定greeter，优先级最低，允许被其他应用以更高的配置文件覆盖
install(FILES files/50-deepin.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/lightdm/lightdm.conf.d)
install(PROGRAMS files/pre-greeter DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/greeters.d)

if (DISABLE_DSS_SNIPE)
    file(GLOB QM_FILES "translations/*.qm")
endif ()
install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/translations)
install(FILES qt-theme.ini DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lightdm/deepin)

# desktop
install(FILES files/x/lightdm-deepin-greeter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/xgreeters/)
if (DISABLE_DSS_SNIPE)
    install(FILES files/dde-lock.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/)
    install(FILES files/dde-lock.desktop DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart/)
else ()
    install(FILES files/snipe/dde-lock.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/)
endif ()

# install conf files
install(FILES files/dde-session-shell.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/)
install(FILES files/lightdm-deepin-greeter.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin-authentication/privileges/)

# services
if (DISABLE_DSS_SNIPE)
    file(GLOB SERVICES files/*.service)
else ()
    file(GLOB SERVICES files/snipe/*.service)
endif ()
install(FILES ${SERVICES} DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

if (DISABLE_DSS_SNIPE)
    #schemas
    install(FILES xml/com.deepin.dde.session-shell.gschema.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas)
endif ()

# dev
file(GLOB INTERFACE interface/*.h)
install(FILES ${INTERFACE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dde-session-shell)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/DdeSessionShellConfig.cmake.in
    ${CMAKE_SOURCE_DIR}/cmake/DdeSessionShellConfig.cmake
    @ONLY)
file(GLOB CMAKE_CONFIG cmake/*.cmake)
install(FILES ${CMAKE_CONFIG} DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DdeSessionShell)

# pam
file(GLOB PAM_CONFIG files/pam.d/*)
install(FILES ${PAM_CONFIG} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d/)

# config
if (DISABLE_DSS_SNIPE)
    dconfig_meta_files(APPID org.deepin.dde.lock FILES configs/org.deepin.dde.lock.json)
    dconfig_meta_files(APPID org.deepin.dde.lightdm-deepin-greeter FILES configs/org.deepin.dde.lightdm-deepin-greeter.json)
else ()
    dtk_add_config_meta_files(APPID org.deepin.dde.lock FILES configs/snipe/org.deepin.dde.lock.json)
    dtk_add_config_meta_files(APPID org.deepin.dde.lightdm-deepin-greeter FILES configs/snipe/org.deepin.dde.lightdm-deepin-greeter.json)
    dtk_add_config_meta_files(APPID org.deepin.dde.session-shell FILES configs/snipe/org.deepin.dde.session-shell.json)
endif ()

# debug config file
install(FILES files/deepin-debug-config/org.deepin.dde.session-shell.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin-debug-config/deepin-debug-config.d)
install(FILES files/deepin-log-config/org.deepin.dde.session-shell.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin-log-viewer/deepin-log.conf.d)
