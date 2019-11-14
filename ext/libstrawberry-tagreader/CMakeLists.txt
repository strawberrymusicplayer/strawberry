include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-common)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -U__STRICT_ANSI__ -Wall -Wextra -Wpedantic -Woverloaded-virtual -fpermissive")

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
endif()

set(MESSAGES
  tagreadermessages.proto
)

set(SOURCES
  fmpsparser.cpp
  tagreader.cpp
)

set(HEADERS
)

qt5_wrap_cpp(MOC ${HEADERS})

protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${MESSAGES})

add_library(libstrawberry-tagreader STATIC
  ${PROTO_SOURCES}
  ${SOURCES}
  ${MOC}
)

target_link_libraries(libstrawberry-tagreader
  ${PROTOBUF_LIBRARY}
  libstrawberry-common
)
