cmake_minimum_required(VERSION 3.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-common)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)

link_directories(${PROTOBUF_LIBRARY_DIRS})
include_directories(${PROTOBUF_INCLUDE_DIRS})

link_directories(${TAGLIB_LIBRARY_DIRS})
include_directories(${TAGLIB_INCLUDE_DIRS})

set(MESSAGES tagreadermessages.proto)
set(SOURCES fmpsparser.cpp tagreader.cpp)

protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${MESSAGES})

add_library(libstrawberry-tagreader STATIC ${PROTO_SOURCES} ${SOURCES})
target_link_libraries(libstrawberry-tagreader ${PROTOBUF_LIBRARY} libstrawberry-common)
