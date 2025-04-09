cmake_minimum_required(VERSION 3.13)
project(assist_Login_interface)

set(CMAKE_CXX_STANDARD 11)

find_package(Qt5 COMPONENTS Network REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(SSL REQUIRED IMPORTED_TARGET libcrypto libssl openssl)

set(LIB_NAME assist_Login_interface)


# 添加动态库源文件
add_library(${LIB_NAME} SHARED include/assist_login_interface.h src/assist_login_interface.cpp)

target_link_libraries(${LIB_NAME}
        Qt5::Network
        ${Qt_LIBS}
        PkgConfig::SSL)

# 将包含目录添加到 mylib 目标中
target_include_directories(${LIB_NAME} PUBLIC include)

link_libraries(

)

# 安装/user/lib
install(FILES include/assist_login_interface.h DESTINATION /usr/include/dde-session-shell)
install(TARGETS ${LIB_NAME} LIBRARY DESTINATION lib/dde-session-shell/modules)
