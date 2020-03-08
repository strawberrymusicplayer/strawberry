set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -U__STRICT_ANSI__ -Wall -Wextra -Wpedantic -Woverloaded-virtual -fpermissive")

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)

if (Backtrace_FOUND)
  include_directories(${Backtrace_INCLUDE_DIRS})
endif(Backtrace_FOUND)

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

add_library(libstrawberry-common STATIC
  ${SOURCES}
  ${MOC}
)

if (Backtrace_FOUND)
  target_link_libraries(libstrawberry-common ${Backtrace_LIBRARIES})
endif (Backtrace_FOUND)

target_link_libraries(libstrawberry-common
  ${TAGLIB_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(libstrawberry-common Qt5::Core Qt5::Network)
