include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -U__STRICT_ANSI__ -Wall -Woverloaded-virtual -Wno-sign-compare -fpermissive")

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

target_link_libraries(libstrawberry-common
  #${PROTOBUF_LIBRARY}
  ${TAGLIB_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(libstrawberry-common Qt5::Core Qt5::Network)
