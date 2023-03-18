cmake_minimum_required(VERSION 3.7)

set(MESSAGES tagreadermessages.proto)
set(SOURCES tagreaderbase.cpp)

if(USE_TAGLIB AND TAGLIB_FOUND)
  list(APPEND SOURCES tagreadertaglib.cpp tagreadergme.cpp)
endif()

if(USE_TAGPARSER AND TAGPARSER_FOUND)
  list(APPEND SOURCES tagreadertagparser.cpp)
endif()

protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${MESSAGES})

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
  ${QtCore_LIBRARIES}
  ${QtNetwork_LIBRARIES}
  ${QtGui_LIBRARIES}
  libstrawberry-common
)

if(WIN32 AND Protobuf_VERSION VERSION_GREATER_EQUAL 4.22.0)
  if (MSVC)
    target_link_libraries(libstrawberry-tagreader PRIVATE abseil_dll)
  else()
    target_link_libraries(libstrawberry-tagreader PRIVATE absl_log_internal_message absl_log_internal_check_op)
  endif()
endif()

if(USE_TAGLIB AND TAGLIB_FOUND)
  target_include_directories(libstrawberry-tagreader SYSTEM PRIVATE ${TAGLIB_INCLUDE_DIRS})
  target_link_libraries(libstrawberry-tagreader PRIVATE ${TAGLIB_LIBRARIES})
endif()

if(USE_TAGPARSER AND TAGPARSER_FOUND)
  target_include_directories(libstrawberry-tagreader SYSTEM PRIVATE ${TAGPARSER_INCLUDE_DIRS})
  target_link_libraries(libstrawberry-tagreader PRIVATE ${TAGPARSER_LIBRARIES})
endif()
