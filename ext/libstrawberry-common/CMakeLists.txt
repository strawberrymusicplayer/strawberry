cmake_minimum_required(VERSION 3.0)

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

if(BUILD_WITH_QT6)
  qt6_wrap_cpp(MOC ${HEADERS})
else()
  qt5_wrap_cpp(MOC ${HEADERS})
endif()

link_directories(${GLIB_LIBRARY_DIRS})

add_library(libstrawberry-common STATIC ${SOURCES} ${MOC})

target_include_directories(libstrawberry-common SYSTEM PRIVATE ${GLIB_INCLUDE_DIRS})
target_include_directories(libstrawberry-common PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}/src
)

if(Backtrace_FOUND)
  target_include_directories(libstrawberry-common SYSTEM PRIVATE ${Backtrace_INCLUDE_DIRS})
endif(Backtrace_FOUND)

target_link_libraries(libstrawberry-common PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  ${QtCore_LIBRARIES}
  ${QtNetwork_LIBRARIES}
)

if(Backtrace_FOUND)
  target_link_libraries(libstrawberry-common PRIVATE ${Backtrace_LIBRARIES})
endif(Backtrace_FOUND)
