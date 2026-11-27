include_directories(
    global
)

link_libraries(
    ${Qt_LIBS}
)

set(LIB_NAME network)

aux_source_directory(. SRCS)

add_library(${LIB_NAME} SHARED ${SRCS})

install(TARGETS ${LIB_NAME} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/dde-session-shell/modules)
