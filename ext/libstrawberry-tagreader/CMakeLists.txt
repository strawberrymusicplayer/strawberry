cmake_minimum_required(VERSION 3.7)

# Workaround a bug in protobuf-generate.cmake (https://github.com/protocolbuffers/protobuf/issues/12450)
if(NOT protobuf_PROTOC_EXE)
  set(protobuf_PROTOC_EXE "protobuf::protoc")
endif()

if(NOT Protobuf_LIBRARIES)
  set(Protobuf_LIBRARIES protobuf::libprotobuf)
endif()

set(SOURCES tagreaderbase.cpp tagreadermessages.proto)

if(USE_TAGLIB AND TAGLIB_FOUND)
  list(APPEND SOURCES tagreadertaglib.cpp tagreadergme.cpp)
endif()

if(USE_TAGPARSER AND TAGPARSER_FOUND)
  list(APPEND SOURCES tagreadertagparser.cpp)
endif()

link_directories(
  ${GLIB_LIBRARY_DIRS}
  ${PROTOBUF_LIBRARY_DIRS}
)

if(USE_TAGLIB AND TAGLIB_FOUND)
  link_directories(${TAGLIB_LIBRARY_DIRS})
endif()

if(USE_TAGPARSER AND TAGPARSER_FOUND)
  link_directories(${TAGPARSER_LIBRARY_DIRS})
endif()

add_library(libstrawberry-tagreader STATIC ${PROTO_SOURCES} ${SOURCES})

target_include_directories(libstrawberry-tagreader SYSTEM PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
)

target_include_directories(libstrawberry-tagreader PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(libstrawberry-tagreader PRIVATE
  ${GLIB_LIBRARIES}
  ${Protobuf_LIBRARIES}
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Gui
  libstrawberry-common
)

if(USE_TAGLIB AND TAGLIB_FOUND)
  target_include_directories(libstrawberry-tagreader SYSTEM PRIVATE ${TAGLIB_INCLUDE_DIRS})
  target_link_libraries(libstrawberry-tagreader PRIVATE ${TAGLIB_LIBRARIES})
endif()

if(USE_TAGPARSER AND TAGPARSER_FOUND)
  target_include_directories(libstrawberry-tagreader SYSTEM PRIVATE ${TAGPARSER_INCLUDE_DIRS})
  target_link_libraries(libstrawberry-tagreader PRIVATE ${TAGPARSER_LIBRARIES})
endif()

protobuf_generate(TARGET libstrawberry-tagreader)
