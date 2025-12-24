include_directories(
    global
)

link_libraries(
    ${Qt_LIBS}
)

set(LIB_NAME login-udcp)

aux_source_directory(. SRCS)

add_library(${LIB_NAME} SHARED ${SRCS} login-plugin.qrc)

install(TARGETS ${LIB_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dde-session-shell/modules)
