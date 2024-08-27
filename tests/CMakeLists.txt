enable_testing()

add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=1)

set(TESTUTILS-SOURCES
  src/mock_networkaccessmanager.cpp
  src/mock_playlistitem.cpp
  src/test_utils.cpp
  src/testobjectdecorators.cpp
)

set(TESTUTILS-MOC-HEADERS src/mock_networkaccessmanager.h src/test_utils.h src/testobjectdecorators.h)

qt_wrap_cpp(TESTUTILS-SOURCES-MOC ${TESTUTILS-MOC-HEADERS})

add_library(test_utils STATIC EXCLUDE_FROM_ALL ${TESTUTILS-SOURCES} ${TESTUTILS-SOURCES-MOC})
target_include_directories(test_utils SYSTEM PRIVATE
  ${GTEST_INCLUDE_DIRS}
  ${GMOCK_INCLUDE_DIRS}
)
target_include_directories(test_utils PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
)
target_link_directories(test_utils PRIVATE
  ${SQLITE_LIBRARY_DIRS}
  ${TAGLIB_LIBRARY_DIRS}
)
target_link_libraries(test_utils PRIVATE
  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARY}
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Sql
  Qt${QT_VERSION_MAJOR}::Test
)

add_custom_target(strawberry_tests echo "Running Strawberry tests" WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_custom_target(build_tests WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_dependencies(strawberry_tests build_tests)

qt_add_resources(TEST-RESOURCE-SOURCES data/testdata.qrc)

add_library(test_gui_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
target_include_directories(test_gui_main PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
)
if(HAVE_GSTREAMER)
  target_include_directories(test_gui_main SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
endif()
target_link_libraries(test_gui_main PRIVATE strawberry_lib)
set_target_properties(test_gui_main PROPERTIES COMPILE_DEFINITIONS GUI)

add_library(test_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
target_include_directories(test_main PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
)
if(HAVE_GSTREAMER)
  target_include_directories(test_main SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
endif()
target_link_libraries(test_main PRIVATE strawberry_lib)

# Given a file foo_test.cpp, creates a target foo_test and adds it to the test target.
macro(add_test_file test_source gui_required)
    get_filename_component(TEST_NAME ${test_source} NAME_WE)
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${test_source})
    target_include_directories(${TEST_NAME} SYSTEM PRIVATE
      ${GTEST_INCLUDE_DIRS}
      ${GMOCK_INCLUDE_DIRS}
    )
    target_include_directories(${TEST_NAME} PRIVATE
      ${CMAKE_BINARY_DIR}/src
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
      ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
      ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
      ${TAGLIB_INCLUDE_DIRS}
    )
    target_link_libraries(${TEST_NAME} PRIVATE
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Concurrent
      Qt${QT_VERSION_MAJOR}::Widgets
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::Sql
      Qt${QT_VERSION_MAJOR}::Test
    )
    target_link_libraries(${TEST_NAME} PRIVATE test_utils)
    set(GUI_REQUIRED ${gui_required})
    if(GUI_REQUIRED)
      target_link_libraries(${TEST_NAME} PRIVATE test_gui_main)
    else()
      target_link_libraries(${TEST_NAME} PRIVATE test_main)
    endif()

    add_test(strawberry_tests ${TEST_NAME})
    add_custom_command(TARGET strawberry_tests POST_BUILD COMMAND ./${TEST_NAME}${CMAKE_EXECUTABLE_SUFFIX})
    add_dependencies(build_tests ${TEST_NAME})
endmacro(add_test_file)

add_test_file(src/utilities_test.cpp false)
add_test_file(src/concurrentrun_test.cpp false)
add_test_file(src/mergedproxymodel_test.cpp false)
add_test_file(src/sqlite_test.cpp false)
add_test_file(src/tagreader_test.cpp false)
add_test_file(src/collectionbackend_test.cpp false)
add_test_file(src/collectionmodel_test.cpp false)
add_test_file(src/songplaylistitem_test.cpp false)
add_test_file(src/organizeformat_test.cpp false)
add_test_file(src/playlist_test.cpp true)

add_custom_target(run_strawberry_tests COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS strawberry_tests)
