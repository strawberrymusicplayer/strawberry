find_package(Qt5 ${QT_MIN_VERSION} COMPONENTS Test)
find_package(GTest)
find_library(GMOCK_LIBRARY gmock)

if(Qt5Test_FOUND AND GTEST_FOUND AND GMOCK_LIBRARY)

  include_directories(${GTEST_INCLUDE_DIRS} ${GMOCK_INCLUDE_DIRS} ${QT_QTTEST_INCLUDE_DIR})
  include_directories(${TAGLIB_INCLUDE_DIRS})
  if(HAVE_GSTREAMER)
    link_directories(${GSTREAMER_LIBRARY_DIRS})
    include_directories(${GSTREAMER_INCLUDE_DIRS})
    include_directories(${GSTREAMER_APP_INCLUDE_DIRS})
    include_directories(${GSTREAMER_AUDIO_INCLUDE_DIRS})
    include_directories(${GSTREAMER_BASE_INCLUDE_DIRS})
    include_directories(${GSTREAMER_TAG_INCLUDE_DIRS})
  endif()
  include_directories(${CMAKE_SOURCE_DIR}/src)
  include_directories(${CMAKE_BINARY_DIR}/src)
  include_directories(${CMAKE_SOURCE_DIR}/tests/src)
  include_directories(${CMAKE_BINARY_DIR}/tests/src)
  include_directories(${CMAKE_SOURCE_DIR}/ext/strawberry-tagreader)
  include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-common)
  include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader)
  include_directories(${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader)
  if(HAVE_LIBGPOD)
    include_directories(${LIBGPOD_INCLUDE_DIRS})
  endif(HAVE_LIBGPOD)

  add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=1)

  set(TESTUTILS-SOURCES
    src/mock_networkaccessmanager.cpp
    src/mock_playlistitem.cpp
    src/test_utils.cpp
    src/testobjectdecorators.cpp
  )

  set(TESTUTILS-MOC-HEADERS src/mock_networkaccessmanager.h src/test_utils.h src/testobjectdecorators.h)

  qt5_wrap_cpp(TESTUTILS-SOURCES-MOC ${TESTUTILS-MOC-HEADERS})

  add_library(test_utils STATIC EXCLUDE_FROM_ALL ${TESTUTILS-SOURCES} ${TESTUTILS-SOURCES-MOC})
  target_link_libraries(test_utils ${GTEST_BOTH_LIBRARIES} ${GMOCK_LIBRARY} ${QT_LIBRARIES} ${Qt5Test_LIBRARIES})

  add_custom_target(strawberry_test echo "Running Strawberry tests" WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
  add_custom_target(build_tests WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
  add_dependencies(strawberry_test build_tests)

  qt5_add_resources(TEST-RESOURCE-SOURCES data/testdata.qrc)

  add_library(test_gui_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
  target_link_libraries(test_gui_main strawberry_lib)
  set_target_properties(test_gui_main PROPERTIES COMPILE_DEFINITIONS GUI)

  add_library(test_main STATIC EXCLUDE_FROM_ALL ${TEST-RESOURCE-SOURCES} src/main.cpp)
  target_link_libraries(test_main strawberry_lib)

  # Given a file foo_test.cpp, creates a target foo_test and adds it to the test target.
  macro(add_test_file test_source gui_required)
      get_filename_component(TEST_NAME ${test_source} NAME_WE)
      add_executable(${TEST_NAME}
        EXCLUDE_FROM_ALL
        ${test_source}
      )
      target_link_libraries(${TEST_NAME} ${GMOCK_LIBRARY} strawberry_lib test_utils Qt5::Test)
      set(GUI_REQUIRED ${gui_required})
      if (GUI_REQUIRED)
        target_link_libraries(${TEST_NAME} test_gui_main)
      else (GUI_REQUIRED)
        target_link_libraries(${TEST_NAME} test_main)
      endif (GUI_REQUIRED)

      check_cxx_compiler_flag("-Wno-bool-conversions" SUPPORTS_NOBOOL)
      if (SUPPORTS_NOBOOL)
        set_target_properties(${TEST_NAME} PROPERTIES COMPILE_FLAGS "-Wno-bool-conversions")
      endif (SUPPORTS_NOBOOL)

      add_custom_command(TARGET strawberry_test POST_BUILD COMMAND ./${TEST_NAME}${CMAKE_EXECUTABLE_SUFFIX})
      add_dependencies(build_tests ${TEST_NAME})
  endmacro (add_test_file)

  add_test_file(src/utilities_test.cpp false)
  add_test_file(src/concurrentrun_test.cpp false)
  add_test_file(src/closure_test.cpp false)
  add_test_file(src/mergedproxymodel_test.cpp false)
  add_test_file(src/sqlite_test.cpp false)
  add_test_file(src/song_test.cpp false)
  add_test_file(src/collectionbackend_test.cpp false)
  add_test_file(src/collectionmodel_test.cpp true)
  add_test_file(src/songplaylistitem_test.cpp false)
  add_test_file(src/organiseformat_test.cpp false)

endif()   # Qt5Test_FOUND AND GTEST_FOUND AND GMOCK_LIBRARY
