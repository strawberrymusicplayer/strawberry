include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-common)
include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader)
include_directories(${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_BINARY_DIR}/src)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -U__STRICT_ANSI__ -Wall -Woverloaded-virtual -Wno-sign-compare -fpermissive")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra -Wpedantic")
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(SOURCES
  main.cpp
  tagreaderworker.cpp
)

qt5_wrap_cpp(MOC ${HEADERS})
qt5_add_resources(QRC data/data.qrc)

add_executable(strawberry-tagreader
  ${SOURCES}
  ${MOC}
  ${QRC}
)

target_link_libraries(strawberry-tagreader
  ${TAGLIB_LIBRARIES}
  ${QT_QTCORE_LIBRARY}
  ${QT_QTNETWORK_LIBRARY}
  libstrawberry-common
  libstrawberry-tagreader
)

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_libraries(strawberry-tagreader
    execinfo
  )
endif(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

if(APPLE)
  target_link_libraries(strawberry-tagreader
    /System/Library/Frameworks/Foundation.framework
  )
endif(APPLE)


install(TARGETS strawberry-tagreader RUNTIME DESTINATION bin)
