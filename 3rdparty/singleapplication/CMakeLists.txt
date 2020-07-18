cmake_minimum_required(VERSION 3.0)

include(CheckIncludeFiles)
include(CheckFunctionExists)

if(CMAKE_VERSION VERSION_GREATER 3.0)
  check_function_exists(geteuid HAVE_GETEUID)
  check_function_exists(getpwuid HAVE_GETPWUID)
endif()

set(SINGLEAPP-SOURCES singleapplication.cpp singleapplication_p.cpp)
set(SINGLEAPP-MOC-HEADERS singleapplication.h singleapplication_p.h)
if(WITH_QT6)
  qt6_wrap_cpp(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
else()
  qt5_wrap_cpp(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
endif()
add_library(singleapplication STATIC ${SINGLEAPP-SOURCES} ${SINGLEAPP-SOURCES-MOC})
target_include_directories(singleapplication SYSTEM PRIVATE
  ${QtCore_INCLUDE_DIRS}
  ${QtWidgets_INCLUDE_DIRS}
  ${QtNetwork_INCLUDE_DIRS}
)
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
if(WITH_QT6)
  qt6_wrap_cpp(SINGLECOREAPP-SOURCES-MOC ${SINGLECOREAPP-MOC-HEADERS})
else()
  qt5_wrap_cpp(SINGLECOREAPP-SOURCES-MOC ${SINGLECOREAPP-MOC-HEADERS})
endif()
add_library(singlecoreapplication STATIC ${SINGLECOREAPP-SOURCES} ${SINGLECOREAPP-SOURCES-MOC})
target_include_directories(singlecoreapplication SYSTEM PRIVATE
  ${QtCore_INCLUDE_DIRS}
  ${QtNetwork_INCLUDE_DIRS}
)
target_include_directories(singlecoreapplication PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(singlecoreapplication PRIVATE
  ${QtCore_LIBRARIES}
  ${QtNetwork_LIBRARIES}
)

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")
