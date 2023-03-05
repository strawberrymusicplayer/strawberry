cmake_minimum_required(VERSION 3.7)

add_definitions(-DSINGLEAPPLICATION)

set(SOURCES ../singleapplication_t.cpp ../singleapplication_p.cpp)
set(HEADERS ../singleapplication_t.h ../singleapplication_p.h)
qt_wrap_cpp(MOC ${HEADERS})
add_library(singleapplication STATIC ${SOURCES} ${MOC})
target_include_directories(singleapplication PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_BINARY_DIR}/..
  ${Boost_INCLUDE_DIRS}
)
target_link_libraries(singleapplication PUBLIC
  ${QtCore_LIBRARIES}
  ${QtWidgets_LIBRARIES}
  ${QtNetwork_LIBRARIES}
)
