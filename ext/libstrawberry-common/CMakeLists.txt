cmake_minimum_required(VERSION 3.0)

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

if(WITH_QT6)
  qt6_wrap_cpp(MOC ${HEADERS})
else()
  qt5_wrap_cpp(MOC ${HEADERS})
endif()

link_directories(
  ${GLIB_LIBRARY_DIRS}
  ${QtCore_LIBRARY_DIRS}
  ${QtNetwork_LIBRARY_DIRS}
)

add_library(libstrawberry-common STATIC ${SOURCES} ${MOC})

target_include_directories(libstrawberry-common SYSTEM PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${QtCore_INCLUDE_DIRS}
  ${QtNetwork_INCLUDE_DIRS}
)

target_include_directories(libstrawberry-common PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(libstrawberry-common PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  ${QtCore_LIBRARIES}
  ${QtNetwork_LIBRARIES}
)

if(Backtrace_FOUND)
  target_include_directories(libstrawberry-common PRIVATE ${Backtrace_INCLUDE_DIRS})
  target_link_libraries(libstrawberry-common PRIVATE ${Backtrace_LIBRARIES})
endif(Backtrace_FOUND)
