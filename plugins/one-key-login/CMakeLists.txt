find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

link_libraries(
    ${Qt_LIBS}
    Dtk${DTK_VERSION_MAJOR}::Core
    Dtk${DTK_VERSION_MAJOR}::Widget
)

set(LIB_NAME one-key-login)

aux_source_directory(. SRCS)

add_library(${LIB_NAME} SHARED ${SRCS})

install(TARGETS ${LIB_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dde-session-shell/modules)
