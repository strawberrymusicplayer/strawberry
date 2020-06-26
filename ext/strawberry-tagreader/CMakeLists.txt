cmake_minimum_required(VERSION 3.0)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(SOURCES main.cpp tagreaderworker.cpp)

qt5_wrap_cpp(MOC ${HEADERS})
qt5_add_resources(QRC data/data.qrc)

link_directories(
  ${GLIB_LIBRARY_DIRS}
  ${TAGLIB_LIBRARY_DIRS}
  ${Qt5Core_LIBRARY_DIRS}
  ${Qt5Network_LIBRARY_DIRS}
)

add_executable(strawberry-tagreader ${SOURCES} ${MOC} ${QRC})

target_include_directories(strawberry-tagreader SYSTEM PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${Qt5Core_INCLUDE_DIRS}
  ${Qt5Network_INCLUDE_DIRS}
)

target_include_directories(strawberry-tagreader PRIVATE
  ${TAGLIB_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(strawberry-tagreader PRIVATE
  ${GLIB_LIBRARIES}
  ${TAGLIB_LIBRARIES}
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
