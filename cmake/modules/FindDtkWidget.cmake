find_package(DtkWidget CONFIG)

if (DTKWIDGET_INCLUDE_DIR AND DtkWidget_LIBRARIES)
    set(DtkWidget_LIBRARIES_FOUND TRUE)
endif (DTKWIDGET_INCLUDE_DIR AND DtkWidget_LIBRARIES)
message(STATUS "Found DtkWidget: ${DtkWidget_LIBRARIES}")
