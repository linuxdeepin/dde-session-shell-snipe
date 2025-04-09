# 设置运行此配置所需的 cmake 的最低版本
cmake_minimum_required(VERSION 3.11)

# 使用 set 命令设置一个变量
set(PLUGIN_NAME login-basic)

# 设置项目名称
project(${PLUGIN_NAME})

# 启用 qt moc 的支持
set(CMAKE_AUTOMOC ON)
# 启用 qrc 资源文件的支持
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Core QUIET)
if (Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
    set(DTK_VERSION_MAJOR 6)
else ()
    set(QT_VERSION_MAJOR 5)
    set(DTK_VERSION_MAJOR 5)
endif ()

# 指定所有源码文件
# 使用了 cmake 的 file 命令，递归查找项目目录下所有头文件和源码文件
# 并将结果放入 SRCS 变量中，方便后续取用
file(GLOB_RECURSE SRCS "*.h" "*.cpp")

# 指定要用到的库
# 使用了 cmake 的 find_package 命令，查找库 Qt对应版本的模块，如Widgets等，
# REQUIRED 参数表示如果没有找到则报错
# find_package 命令在找到并加载指定的库之后会设置一些变量
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Dtk${DTK_VERSION_MAJOR} REQUIRED COMPONENTS Widget)
find_package(DdeSessionShell REQUIRED)

# find_package 命令还可以用来加载 cmake 的功能模块
# 并不是所有的库都直接支持 cmake 查找的，但大部分都支持了 pkg-config 这个标准，
# 因此 cmake 提供了间接加载库的模块：FindPkgConfig， 下面这行命令表示加载 FindPkgConfig 模块，
# 这个 cmake 模块提供了额外的基于 “pkg-config” 加载库的能力
# 执行下面的命令后后会设置如下变量，不过一般用不到：
# PKG_CONFIG_FOUND            pkg-config 可执行文件是否找到了
# PKG_CONFIG_EXECUTABLE       pkg-config 可执行文件的路径
# PKG_CONFIG_VERSION_STRING   pkg-config 的版本信息
find_package(PkgConfig REQUIRED)

# add_definitions 命令用于声明/定义一些编译/预处理参数
add_definitions("${QT_DEFINITIONS} -DQT_PLUGIN")

# 新增一个编译目标
# 这里使用命令 add_library 来表示本项目要生成一个库文件目标，
# 类似的还有命令 add_executable 添加一个可执行二进制目标，甚至 add_custom_target(使用较少) 添加自定义目标
# SHARED 表示生成的库应该是动态库，
# 变量 ${PLUGIN_NAME} 和 ${SRCS} 都是前面处理好的，
# 另外 qrc 资源文件也应该追加在后面以编译进目标中。
add_library(${PLUGIN_NAME} SHARED ${SRCS})

# 设置目标的生成位置，这里表示生成在执行 make 的目录,
# 另外还有很多可用于设置的属性，可查阅 cmake 文档。
set_target_properties(${PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ./)

# 设置目标要使用的 include 目录，即头文件目录
# 变量 ${DtkWidget_INCLUDE_DIRS} 是在前面执行 find_package 命令时引入的
# 当出现编译失败提示找不到某些库的头文件时应该检查此处是否将所有需要的头文件都包含了
target_include_directories(${PLUGIN_NAME} PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    ${DtkWidget_INCLUDE_DIRS}
    ${DdeSessionShell_INCLUDE_DIRS}
)

# 设置目标要使用的链接库
# 变量 Dtk${DTK_VERSION_MAJOR}::Widget 和 Qt${QT_VERSION_MAJOR}::Widgets 是在前面执行 find_package 命令时引入的
# 当出现运行时错误提示某些符号没有定义时应该检查此处是否将所有用的库都写在了这里
target_link_libraries(${PLUGIN_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Dtk${DTK_VERSION_MAJOR}::Widget
    ${DdeSessionShell_LIBRARIES}
)

# 设置安装路径的前缀(默认为"/usr/local")
set(CMAKE_INSTALL_PREFIX "/usr")

# 设置执行 make install 时哪个目标应该被 install 到哪个位置
install(TARGETS ${PLUGIN_NAME} LIBRARY DESTINATION lib/dde-session-shell/modules)
