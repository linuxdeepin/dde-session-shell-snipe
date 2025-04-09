project(lightdm-deepin-greeter-lighter)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(DtkWidget REQUIRED)

# Find the library
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11 xi xcursor xfixes xrandr xext xtst)
pkg_check_modules(Greeter REQUIRED liblightdm-qt5-3)
pkg_check_modules(DFrameworkDBus REQUIRED dframeworkdbus)
pkg_check_modules(QGSettings REQUIRED gsettings-qt)

add_executable(${PROJECT_NAME}
    "main.cpp"
    "lighterbackground.h"
    "lighterbackground.cpp"
    "lightergreeter.h"
    "lightergreeter.cpp"
    "lighterscreenmanager.h"
    "lighterscreenmanager.cpp"
    "../src/widgets/abstractfullbackgroundinterface.h"
    "../src/widgets/timewidget.h"
    "../src/widgets/timewidget.cpp"
    "../src/widgets/dlineeditex.h"
    "../src/widgets/dlineeditex.cpp"
    "../src/widgets/transparentbutton.h"
    "../src/widgets/transparentbutton.cpp"
    "../src/widgets/useravatar.h"
    "../src/widgets/useravatar.cpp"
    "../src/global_util/constants.h"
    "../src/global_util/public_func.h"
    "../src/global_util/public_func.cpp"
    "../src/global_util/dconfig_helper.h"
    "../src/global_util/dconfig_helper.cpp"
    "../resources.qrc"
    )

target_include_directories(${PROJECT_NAME} PUBLIC
    ${DtkWidget_INCLUDE_DIRS}
    ${Greeter_INCLUDE_DIRS}
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${QGSettings_INCLUDE_DIRS}
    ${XCB_EWMH_INCLUDE_DIRS}
    )

target_link_libraries(${PROJECT_NAME}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${DtkCore_LIBRARIES}
    ${Greeter_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${QGSettings_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    )

install(TARGETS lightdm-deepin-greeter-lighter DESTINATION ${CMAKE_INSTALL_BINDIR})
