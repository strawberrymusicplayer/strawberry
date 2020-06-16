cmake_minimum_required(VERSION 3.0)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(SOURCES main.cpp tagreaderworker.cpp)

qt5_wrap_cpp(MOC ${HEADERS})
qt5_add_resources(QRC data/data.qrc)

add_executable(strawberry-tagreader ${SOURCES} ${MOC} ${QRC})

target_include_directories(strawberry-tagreader PRIVATE
  ${PROTOBUF_INCLUDE_DIRS}
  ${TAGLIB_INCLUDE_DIRS}
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Network_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/src
)

target_link_directories(strawberry-tagreader PRIVATE
  ${PROTOBUF_LIBRARY_DIRS}
  ${TAGLIB_LIBRARY_DIRS}
  ${Qt5Core_LIBRARY_DIRS}
  ${Qt5Network_LIBRARY_DIRS}
)

target_link_libraries(strawberry-tagreader PRIVATE
  ${Qt5Core_LIBRARIES}
  ${Qt5Network_LIBRARIES}
  libstrawberry-common
  libstrawberry-tagreader
)

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_libraries(strawberry-tagreader PRIVATE execinfo)
endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

if(APPLE)
  target_link_libraries(strawberry-tagreader PRIVATE /System/Library/Frameworks/Foundation.framework)
endif(APPLE)

install(TARGETS strawberry-tagreader RUNTIME DESTINATION bin)
