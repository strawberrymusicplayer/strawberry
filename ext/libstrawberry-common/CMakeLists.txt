cmake_minimum_required(VERSION 3.13)

set(SOURCES
  core/logging.cpp
  core/messagehandler.cpp
  core/messagereply.cpp
  core/workerpool.cpp
)

set(HEADERS
  core/logging.h
  core/messagehandler.h
  core/messagereply.h
  core/workerpool.h
)

qt_wrap_cpp(MOC ${HEADERS})

add_library(libstrawberry-common STATIC ${SOURCES} ${MOC})

target_include_directories(libstrawberry-common SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_include_directories(libstrawberry-common PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/src
)

if(Backtrace_FOUND)
  target_include_directories(libstrawberry-common SYSTEM PRIVATE ${Backtrace_INCLUDE_DIRS})
endif()

target_link_directories(libstrawberry-common PRIVATE ${GLIB_LIBRARY_DIRS})

target_link_libraries(libstrawberry-common PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
)

if(Backtrace_FOUND)
  target_link_libraries(libstrawberry-common PRIVATE ${Backtrace_LIBRARIES})
endif()
