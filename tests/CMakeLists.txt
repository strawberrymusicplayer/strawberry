enable_testing()

add_custom_target(strawberry_tests echo "Running Strawberry tests" WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_custom_target(build_tests WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_dependencies(strawberry_tests build_tests)

add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=1)

set(TESTUTILS-SOURCES
  src/mock_networkaccessmanager.cpp
  src/mock_playlistitem.cpp
  src/test_utils.cpp
)

set(TESTUTILS-MOC-HEADERS src/mock_networkaccessmanager.h src/test_utils.h)

qt_wrap_cpp(TESTUTILS-SOURCES-MOC ${TESTUTILS-MOC-HEADERS})

add_library(test_utils STATIC EXCLUDE_FROM_ALL ${TESTUTILS-SOURCES} ${TESTUTILS-SOURCES-MOC})
target_include_directories(test_utils PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_utils PRIVATE
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Sql
  Qt${QT_VERSION_MAJOR}::Test
)

qt_add_resources(TEST-RESOURCE-SOURCES data/testdata.qrc ${CMAKE_SOURCE_DIR}/data/data.qrc ${CMAKE_SOURCE_DIR}/data/icons.qrc)

add_library(test_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
target_include_directories(test_main PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_main PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  PkgConfig::GLIB
  PkgConfig::GOBJECT
  PkgConfig::GSTREAMER_BASE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  strawberry_lib
)

add_library(test_gui_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
target_include_directories(test_gui_main PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_gui_main PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  PkgConfig::GLIB
  PkgConfig::GOBJECT
  PkgConfig::GSTREAMER_BASE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Widgets
  strawberry_lib
)
set_target_properties(test_gui_main PROPERTIES COMPILE_DEFINITIONS GUI)

# Given a file foo_test.cpp, creates a target foo_test and adds it to the test target.
macro(add_test_file test_source gui_required)
    get_filename_component(TEST_NAME ${test_source} NAME_WE)
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${test_source})
    target_include_directories(${TEST_NAME} PRIVATE
      ${CMAKE_BINARY_DIR}/src
      ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(${TEST_NAME} PRIVATE
      ${CMAKE_THREAD_LIBS_INIT}
      PkgConfig::GLIB
      PkgConfig::GOBJECT
      PkgConfig::GSTREAMER_BASE
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Concurrent
      Qt${QT_VERSION_MAJOR}::Network
      Qt${QT_VERSION_MAJOR}::Sql
      Qt${QT_VERSION_MAJOR}::Test
      Qt${QT_VERSION_MAJOR}::Widgets
    )
    target_link_libraries(${TEST_NAME} PRIVATE test_utils)
    set(GUI_REQUIRED ${gui_required})
    if(GUI_REQUIRED)
      target_link_libraries(${TEST_NAME} PRIVATE test_gui_main)
    else()
      target_link_libraries(${TEST_NAME} PRIVATE test_main)
    endif()

    add_test(NAME ${TEST_NAME} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME}${CMAKE_EXECUTABLE_SUFFIX})
    add_custom_command(TARGET strawberry_tests POST_BUILD COMMAND ./${TEST_NAME}${CMAKE_EXECUTABLE_SUFFIX})
    add_dependencies(build_tests ${TEST_NAME})
endmacro(add_test_file)

add_test_file(src/utilities_test.cpp false)
add_test_file(src/concurrentrun_test.cpp false)
add_test_file(src/mutex_protected_test.cpp false)
add_test_file(src/mergedproxymodel_test.cpp false)
add_test_file(src/sqlite_test.cpp false)
add_test_file(src/tagreader_test.cpp false)
add_test_file(src/collectionbackend_test.cpp false)
add_test_file(src/collectionmodel_test.cpp true)
add_test_file(src/songplaylistitem_test.cpp false)
add_test_file(src/organizeformat_test.cpp false)
add_test_file(src/playlist_test.cpp true)

add_custom_target(run_strawberry_tests COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS strawberry_tests)
