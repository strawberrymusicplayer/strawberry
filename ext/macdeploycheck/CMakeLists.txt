qt_wrap_cpp(MACDEPLOYCHECK_MOC ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common/core/logging.h)
add_executable(macdeploycheck macdeploycheck.cpp ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common/core/logging.cpp ${MACDEPLOYCHECK_MOC})
link_directories(macdeploycheck ${GLIB_LIBRARY_DIRS})
target_include_directories(macdeploycheck PUBLIC SYSTEM
  ${GLIB_INCLUDE_DIRS}
)
target_include_directories(macdeploycheck PUBLIC
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_BINARY_DIR}/src
)
target_link_libraries(macdeploycheck PUBLIC
  "-framework AppKit"
  ${GLIB_LIBRARIES}
  ${QtCore_LIBRARIES}
)
