cmake_minimum_required(VERSION 3.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)

if (Backtrace_FOUND)
  include_directories(${Backtrace_INCLUDE_DIRS})
endif(Backtrace_FOUND)

include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GLIBCONFIG_INCLUDE_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})

set(SOURCES
  core/closure.cpp
  core/logging.cpp
  core/messagehandler.cpp
  core/messagereply.cpp
  core/waitforsignal.cpp
  core/workerpool.cpp
)

set(HEADERS
  core/closure.h
  core/messagehandler.h
  core/messagereply.h
  core/workerpool.h
)

if(APPLE)
  list(APPEND SOURCES core/scoped_nsautorelease_pool.mm)
endif(APPLE)

qt5_wrap_cpp(MOC ${HEADERS})

add_library(libstrawberry-common STATIC ${SOURCES} ${MOC})

if(Backtrace_FOUND)
  target_link_libraries(libstrawberry-common ${Backtrace_LIBRARIES})
endif(Backtrace_FOUND)

target_link_libraries(libstrawberry-common ${TAGLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(libstrawberry-common Qt5::Core Qt5::Network)
