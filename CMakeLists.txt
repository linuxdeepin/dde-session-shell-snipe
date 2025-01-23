cmake_minimum_required(VERSION 3.16)
project(dde-session-shell C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -Wall -pthread -Wl,--as-need -Wl,-E")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# 增加安全编译参数
set(SECURITY_COMPILE, "-fstack-protector-all -fPIC")
set(CMAKE_CXX_FLAGS, "${CMAKE_CXX_FLAGS} ${SECURITY_COMPILE}")
set(CMAKE_C_FLAGS, "${CMAKE_C_FLAGS} ${SECURITY_COMPILE}")

# coverage option
# cmake -DENABLE_COVERAGE=ON ..
OPTION (ENABLE_COVERAGE "Use gcov" OFF)
option (CONTRAST_BACKGROUND "Fill background using high-contrast color for debug" OFF)
MESSAGE(STATUS ENABLE_COVERAGE=${ENABLE_COVERAGE})
if (ENABLE_COVERAGE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    #SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -march=loongson3a -mhard-float -mno-micromips -mno-mips16 -flax-vector-conversions -mloongson-ext2 -mloongson-mmi")
endif()

set(QT_VERSION_MAJOR 6)
set(DTK_VERSION_MAJOR 6)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()
include(GNUInstallDirs)

# Find the library
find_package(PAM REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} ${REQUIRED_QT_VERSION} REQUIRED COMPONENTS Core Widgets Concurrent DBus Xml Svg Network LinguistTools)
find_package(Dtk${DTK_VERSION_MAJOR} REQUIRED COMPONENTS Widget Core Tools)

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11 xi xcursor xfixes xrandr xext xtst)
pkg_check_modules(Greeter REQUIRED liblightdm-qt6-3)

## qm files
file(GLOB TS_FILES "translations/*.ts")
qt_add_translation(QM_FILES ${TS_FILES})
add_custom_target(dde-session-shell_language ALL DEPENDS ${QM_FILES})

set(Qt_LIBS
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Svg
)

set(XML2CPP_DIR ${PROJECT_SOURCE_DIR}/src/global_util/dbus)

function(generation_dbus_interface xml class_file)
    set(file_name ${XML2CPP_DIR}/${class_file})
    execute_process(
            COMMAND ${DTK_XML2CPP} -p ${file_name} ${xml}
            WORKING_DIRECTORY ${XML2CPP_DIR}
    )
endfunction()

set(DBUS_INTERFACES
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.Authenticate1.xml authenticate1interface"
        "${CMAKE_SOURCE_DIR}/xml/com.huawei.switchos.xml switchosinterface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.Accounts1.xml accounts1interface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.Accounts1.User.xml userinterface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.ImageEffect1.xml imageeffect1interface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.Logined.xml loginedinterface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.PowerManager1.xml powermanager1interface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.SystemPower1.xml systempower1interface"
        "${CMAKE_SOURCE_DIR}/xml/com.deepin.wm.xml wminterface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.Authenticate1.Session2.xml session2interface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.SessionManager1.xml sessionmanager1interface"
        "${CMAKE_SOURCE_DIR}/xml/org.deepin.dde.SystemDisplay1.xml systemdisplay1interface"
        "${CMAKE_SOURCE_DIR}/xml/org.freedesktop.DBus.xml dbusinterface"
        "${CMAKE_SOURCE_DIR}/xml/org.freedesktop.login1.session.self.xml selfinterface"
)

foreach(interface IN LISTS DBUS_INTERFACES)
    string(REPLACE " " ";" interface_list "${interface}")

    list(GET interface_list 0 XML_FILE)
    list(GET interface_list 1 CLASS_FILE)

    list(LENGTH interface_list list_len)
    if(list_len LESS 2)
        message(FATAL_ERROR "Invalid DBus interface definition: ${interface}")
    endif()

    generation_dbus_interface(${XML_FILE} ${CLASS_FILE})
endforeach()

include_directories(${PROJECT_SOURCE_DIR}/src/global_util)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/dbus)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/keyboardmonitor)
include_directories(${PROJECT_SOURCE_DIR}/src/widgets)
include_directories(${PROJECT_SOURCE_DIR}/src/session-widgets)
include_directories(${PROJECT_SOURCE_DIR}/src/libdde-auth)
include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/src/global_util/dbus/types)
include_directories(${DTKCORE_INCLUDE_DIRS})

aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util GLOBAL_UTILS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/dbus GLOBAL_UTILS_DBUS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/keyboardmonitor GLOBAL_UTILS_KEYBOARDMONITOR)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/widgets WIDGETS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/session-widgets SESSION_WIDGETS)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/libdde-auth AUTHENTICATE)
aux_source_directory(${PROJECT_SOURCE_DIR}/interface INTERFACE)
aux_source_directory(${PROJECT_SOURCE_DIR}/src/global_util/dbus/types DBUS_DATA_TYPES)

set(QRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${PROJECT_SOURCE_DIR}/src/widgets/widgetsimages.qrc
    ${PROJECT_SOURCE_DIR}/src/widgets/widgetstheme.qrc
)

set(LOCK_SRCS
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${GLOBAL_UTILS_KEYBOARDMONITOR}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${AUTHENTICATE}
    ${INTERFACE}
    ${QT_DBUS_EXTENDED}
    ${DBUS_DATA_TYPES}
    src/app/dde-lock.cpp
    src/dde-lock/lockframe.cpp
    src/dde-lock/lockworker.cpp
    src/dde-lock/dbus/dbuslockagent.cpp
    src/dde-lock/dbus/dbuslockfrontservice.cpp
    src/dde-lock/dbus/dbusshutdownagent.cpp
    src/dde-lock/dbus/dbusshutdownfrontservice.cpp
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
    ${Greeter_INCLUDE_DIRS}
)

target_link_libraries(dde-lock PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    Dtk${DTK_VERSION_MAJOR}::Widget
    Dtk${DTK_VERSION_MAJOR}::Core
    ${Greeter_LIBRARIES}
    ${DEEPIN_PW_CHECK}
)

set(DEEPIN_PW_CHECK libdeepin_pw_check.so)

set(GREETER_SRCS
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${GLOBAL_UTILS_KEYBOARDMONITOR}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${AUTHENTICATE}
    ${INTERFACE}
    ${DBUS_DATA_TYPES}
    src/app/lightdm-deepin-greeter.cpp
    src/lightdm-deepin-greeter/loginframe.cpp
    src/lightdm-deepin-greeter/greeterworker.cpp
    src/lightdm-deepin-greeter/logincontent.cpp
    src/lightdm-deepin-greeter/logintipswindow.cpp
    src/lightdm-deepin-greeter/pwqualitymanager.cpp
    src/lightdm-deepin-greeter/passwordlevelwidget.cpp
    src/lightdm-deepin-greeter/changepasswordwidget.cpp
)
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
    ${DEEPIN_PW_CHECK}
)

if(CONTRAST_BACKGROUND)
    target_compile_definitions(dde-lock PRIVATE CONTRAST_BACKGROUND)
    target_compile_definitions(lightdm-deepin-greeter PRIVATE CONTRAST_BACKGROUND)
endif()

# plugins
add_subdirectory(modules)

add_subdirectory(tests)

if(NOT DSS_CHECK_ACCESSIBILITY)
    set_source_files_properties(${PROJECT_SOURCE_DIR}/src/global_util/accessibilitycheckerex.h 
    TARGET_DIRECTORY dde-lock dde-lock-test lightdm-deepin-greeter lightdm-deepin-greeter-test
    PROPERTIES SKIP_AUTOMOC ON)
else()
    add_compile_definitions(-DDSS_CHECK_ACCESSIBILITY)
endif()

# bin
install(TARGETS dde-lock lightdm-deepin-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES files/deepin-greeter DESTINATION ${CMAKE_INSTALL_BINDIR})

file(GLOB SCRIPTS  scripts/*)
install(PROGRAMS ${SCRIPTS} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/deepin/greeters.d)

install(FILES ${QM_FILES} DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/translations)
install(FILES qt-theme.ini DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/lightdm/deepin)

install(FILES files/lightdm-deepin-greeter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/xgreeters/)
install(FILES files/dde-lock.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications/)
install(FILES files/dde-session-shell.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/dde-session-shell/)
install(FILES files/lightdm-deepin-greeter.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin-authentication/privileges/)

# services
file(GLOB SERVICES files/*.service)
install(FILES ${SERVICES} DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

# dev
file(GLOB INTERFACE interface/*.h)
install(FILES ${INTERFACE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dde-session-shell)

file(GLOB CMAKE_CONFIG cmake/*.cmake)
install(FILES ${CMAKE_CONFIG} DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DdeSessionShell)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/DdeSessionShellConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DdeSessionShellConfig.cmake
    @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/DdeSessionShellConfig.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DdeSessionShell)

# pam
file(GLOB PAM_CONFIG files/pam.d/*)
install(FILES ${PAM_CONFIG} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/pam.d/)

# config
dtk_add_config_meta_files(APPID org.deepin.dde.lock FILES configs/org.deepin.dde.lock.json)
dtk_add_config_meta_files(APPID org.deepin.dde.lightdm-deepin-greeter FILES configs/org.deepin.dde.lightdm-deepin-greeter.json)
dtk_add_config_meta_files(APPID org.deepin.dde.session-shell FILES configs/org.deepin.dde.session-shell.json)
