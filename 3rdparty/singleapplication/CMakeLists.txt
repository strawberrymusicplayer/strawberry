cmake_minimum_required(VERSION 3.7)

include(CheckIncludeFiles)
include(CheckFunctionExists)

check_function_exists(geteuid HAVE_GETEUID)
check_function_exists(getpwuid HAVE_GETPWUID)

set(SINGLEAPP-SOURCES singleapplication.cpp singleapplication_p.cpp)
set(SINGLEAPP-MOC-HEADERS singleapplication.h singleapplication_p.h)
qt_wrap_cpp(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
add_library(singleapplication STATIC ${SINGLEAPP-SOURCES} ${SINGLEAPP-SOURCES-MOC})
target_include_directories(singleapplication PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(singleapplication PRIVATE
  ${QtCore_LIBRARIES}
  ${QtWidgets_LIBRARIES}
  ${QtNetwork_LIBRARIES}
)

set(SINGLECOREAPP-SOURCES singlecoreapplication.cpp singlecoreapplication_p.cpp)
set(SINGLECOREAPP-MOC-HEADERS singlecoreapplication.h singlecoreapplication_p.h)
qt_wrap_cpp(SINGLECOREAPP-SOURCES-MOC ${SINGLECOREAPP-MOC-HEADERS})
add_library(singlecoreapplication STATIC ${SINGLECOREAPP-SOURCES} ${SINGLECOREAPP-SOURCES-MOC})
target_include_directories(singlecoreapplication PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(singlecoreapplication PRIVATE
  ${QtCore_LIBRARIES}
  ${QtNetwork_LIBRARIES}
)

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")
