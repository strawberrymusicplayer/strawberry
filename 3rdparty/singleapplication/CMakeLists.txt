cmake_minimum_required(VERSION 3.0)

include(CheckIncludeFiles)
include(CheckFunctionExists)

if(CMAKE_VERSION VERSION_GREATER 3.0)
  check_function_exists(geteuid HAVE_GETEUID)
  check_function_exists(getpwuid HAVE_GETPWUID)
endif()

set(SINGLEAPP-SOURCES singleapplication.cpp singleapplication_p.cpp)
set(SINGLEAPP-MOC-HEADERS singleapplication.h singleapplication_p.h)
QT5_WRAP_CPP(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
add_library(singleapplication STATIC ${SINGLEAPP-SOURCES} ${SINGLEAPP-SOURCES-MOC})
target_link_libraries(singleapplication Qt5::Core Qt5::Widgets Qt5::Network)

set(SINGLECOREAPP-SOURCES singlecoreapplication.cpp singlecoreapplication_p.cpp)
set(SINGLECOREAPP-MOC-HEADERS singlecoreapplication.h singlecoreapplication_p.h)
QT5_WRAP_CPP(SINGLECOREAPP-SOURCES-MOC ${SINGLECOREAPP-MOC-HEADERS})
add_library(singlecoreapplication STATIC ${SINGLECOREAPP-SOURCES} ${SINGLECOREAPP-SOURCES-MOC})
target_link_libraries(singlecoreapplication Qt5::Core Qt5::Widgets Qt5::Network)

configure_file(config.h.in "${CMAKE_CURRENT_BINARY_DIR}/config.h")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
