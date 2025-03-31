set(PAM_LIB_NAME "pam_inhibit_autologin")

file(GLOB_RECURSE LIB_SRCS
    "*.h"
    "*.c"
)

add_library(${PAM_LIB_NAME} MODULE
    ${LIB_SRCS}
)
set_target_properties(${PAM_LIB_NAME} PROPERTIES PREFIX "")

target_link_libraries(${PAM_LIB_NAME} PRIVATE
    -lpam
)
install(TARGETS ${PAM_LIB_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/security)