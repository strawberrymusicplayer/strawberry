enable_testing()

if(WITH_QT6)
  find_package(Qt6 COMPONENTS Test REQUIRED)
  set(QtTest_LIBRARIES Qt6::Test)
else()
  find_package(Qt5 ${QT_MIN_VERSION} COMPONENTS Test REQUIRED)
  set(QtTest_LIBRARIES ${Qt5Test_LIBRARIES})
endif()

find_package(GTest REQUIRED)
find_library(GMOCK_LIBRARY gmock REQUIRED)

add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=1)

set(TESTUTILS-SOURCES
  src/mock_networkaccessmanager.cpp
  src/mock_playlistitem.cpp
  src/test_utils.cpp
  src/testobjectdecorators.cpp
)

set(TESTUTILS-MOC-HEADERS src/mock_networkaccessmanager.h src/test_utils.h src/testobjectdecorators.h)

if(WITH_QT6)
  qt6_wrap_cpp(TESTUTILS-SOURCES-MOC ${TESTUTILS-MOC-HEADERS})
else()
  qt5_wrap_cpp(TESTUTILS-SOURCES-MOC ${TESTUTILS-MOC-HEADERS})
endif()

link_directories(
  ${SQLITE_LIBRARY_DIRS}
  ${TAGLIB_LIBRARY_DIRS}
  ${QT_LIBRARY_DIRS}
)

if(HAVE_GSTREAMER)
  link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMER_BASE_LIBRARY_DIRS}
    ${GSTREAMER_APP_LIBRARY_DIRS}
    ${GSTREAMER_AUDIO_LIBRARY_DIRS}
    ${GSTREAMER_TAG_LIBRARY_DIRS}
    ${GSTREAMER_PBUTILS_LIBRARY_DIRS}
  )
endif(HAVE_GSTREAMER)

if(HAVE_CHROMAPRINT)
  link_directories(${CHROMAPRINT_LIBRARY_DIRS})
endif(HAVE_CHROMAPRINT)

if(HAVE_AUDIOCD)
  link_directories(${LIBCDIO_LIBRARY_DIRS})
endif(HAVE_AUDIOCD)

if(HAVE_LIBMTP)
  link_directories(${LIBMTP_LIBRARY_DIRS})
endif(HAVE_LIBMTP)

add_library(test_utils STATIC EXCLUDE_FROM_ALL ${TESTUTILS-SOURCES} ${TESTUTILS-SOURCES-MOC})
target_include_directories(test_utils SYSTEM PRIVATE
  ${GTEST_INCLUDE_DIRS}
  ${GMOCK_INCLUDE_DIRS}
  ${QT_INCLUDE_DIRS}
)
target_include_directories(test_utils PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
)
target_link_libraries(test_utils PRIVATE
  ${GTEST_BOTH_LIBRARIES}
  ${GMOCK_LIBRARY}
  ${QtCore_LIBRARIES}
  ${QtWidgets_LIBRARIES}
  ${QtNetwork_LIBRARIES}
  ${QtSql_LIBRARIES}
  ${QtTest_LIBRARIES}
)

add_custom_target(strawberry_tests echo "Running Strawberry tests" WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_custom_target(build_tests WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_dependencies(strawberry_tests build_tests)

if(WITH_QT6)
  qt6_add_resources(TEST-RESOURCE-SOURCES data/testdata.qrc)
else()
  qt5_add_resources(TEST-RESOURCE-SOURCES data/testdata.qrc)
endif()

add_library(test_gui_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
target_include_directories(test_gui_main SYSTEM PRIVATE ${QT_INCLUDE_DIRS})
target_include_directories(test_gui_main PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
)
if(HAVE_GSTREAMER)
  target_include_directories(test_gui_main SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
endif(HAVE_GSTREAMER)
target_link_libraries(test_gui_main PRIVATE strawberry_lib)
set_target_properties(test_gui_main PROPERTIES COMPILE_DEFINITIONS GUI)

add_library(test_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
target_include_directories(test_main SYSTEM PRIVATE ${QT_INCLUDE_DIRS})
target_include_directories(test_main PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
)
if(HAVE_GSTREAMER)
  target_include_directories(test_main SYSTEM PRIVATE ${GSTREAMER_INCLUDE_DIRS})
endif(HAVE_GSTREAMER)
target_link_libraries(test_main PRIVATE strawberry_lib)

# Given a file foo_test.cpp, creates a target foo_test and adds it to the test target.
macro(add_test_file test_source gui_required)
    get_filename_component(TEST_NAME ${test_source} NAME_WE)
    add_executable(${TEST_NAME} EXCLUDE_FROM_ALL ${test_source})
    target_include_directories(${TEST_NAME} SYSTEM PRIVATE
      ${GTEST_INCLUDE_DIRS}
      ${GMOCK_INCLUDE_DIRS}
      ${QT_INCLUDE_DIRS}
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
      ${QtCore_LIBRARIES}
      ${QtWidgets_LIBRARIES}
      ${QtNetwork_LIBRARIES}
      ${QtSql_LIBRARIES}
      ${QtTest_LIBRARIES}
    )
    target_link_libraries(${TEST_NAME} PRIVATE test_utils)
    set(GUI_REQUIRED ${gui_required})
    if(GUI_REQUIRED)
      target_link_libraries(${TEST_NAME} PRIVATE test_gui_main)
    else(GUI_REQUIRED)
      target_link_libraries(${TEST_NAME} PRIVATE test_main)
    endif(GUI_REQUIRED)

    add_test(strawberry_tests ${TEST_NAME})
    add_custom_command(TARGET strawberry_tests POST_BUILD COMMAND ./${TEST_NAME}${CMAKE_EXECUTABLE_SUFFIX})
    add_dependencies(build_tests ${TEST_NAME})
endmacro(add_test_file)

add_test_file(src/utilities_test.cpp false)
add_test_file(src/concurrentrun_test.cpp false)
add_test_file(src/closure_test.cpp false)
add_test_file(src/mergedproxymodel_test.cpp false)
add_test_file(src/sqlite_test.cpp false)
add_test_file(src/tagreader_test.cpp false)
add_test_file(src/collectionbackend_test.cpp false)
add_test_file(src/collectionmodel_test.cpp true)
add_test_file(src/songplaylistitem_test.cpp false)
add_test_file(src/organizeformat_test.cpp false)
add_test_file(src/playlist_test.cpp true)

add_custom_target(run_strawberry_tests COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS strawberry_tests)
