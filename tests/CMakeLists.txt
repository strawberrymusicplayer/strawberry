add_custom_target(strawberry_tests echo "Running Strawberry tests" WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_custom_target(build_tests WORKING_DIRECTORY ${CURRENT_BINARY_DIR})
add_dependencies(strawberry_tests build_tests)

add_definitions(-DGTEST_USE_OWN_TR1_TUPLE=1)
# strawberry_lib is linked privately, its headers have members which depend on this, so the tests need it too.
if(ENABLE_NAS)
  add_definitions(-DHAVE_NAS)
endif()

set(TESTUTILS-SOURCES
  src/environmentvariablerestorer.cpp
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
      PkgConfig::GSTREAMER_APP
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
add_test_file(src/smartplaylistsmodel_test.cpp true)
add_test_file(src/networkproxyfactory_test.cpp false)
add_test_file(src/localredirectserver_test.cpp true)
add_test_file(src/oauthenticator_test.cpp true)
qt_wrap_cpp(OAUTHENTICATOR-TEST-MOC src/oauthenticator_test.h)
target_sources(oauthenticator_test PRIVATE ${OAUTHENTICATOR-TEST-MOC})
if(NOT ANDROID)
  add_test_file(src/lastfmscrobbler_test.cpp true)
  target_sources(lastfmscrobbler_test PRIVATE ${OAUTHENTICATOR-TEST-MOC})
endif()
if(HAVE_QOBUZ AND NOT ANDROID)
  add_test_file(src/qobuzservice_test.cpp true)
  qt_wrap_cpp(QOBUZ-TEST-MOC src/qobuzservice_test.h)
  target_sources(qobuzservice_test PRIVATE ${QOBUZ-TEST-MOC})
  add_test_file(src/qobuzautosave_test.cpp true)
  target_sources(qobuzautosave_test PRIVATE ${CMAKE_SOURCE_DIR}/src/settings/streamingautosave_android.cpp)
  target_include_directories(qobuzautosave_test PRIVATE ${CMAKE_BINARY_DIR})
endif()
add_test_file(src/settingspageautosave_test.cpp true)
target_sources(settingspageautosave_test PRIVATE ${CMAKE_SOURCE_DIR}/src/settings/streamingautosave_android.cpp)
add_test_file(src/filemanagerutils_test.cpp true)
add_test_file(src/song_test.cpp false)
add_test_file(src/diskutils_test.cpp false)
add_test_file(src/database_test.cpp false)
add_test_file(src/devicedatabasebackend_test.cpp false)
add_test_file(src/scopedtransaction_test.cpp false)
add_test_file(src/threadsafenetworkdiskcache_test.cpp false)
add_test_file(src/networkaccessmanager_test.cpp false)
add_test_file(src/temporaryfile_test.cpp false)
add_test_file(src/transcoder_test.cpp false)
add_test_file(src/transcoderoptions_test.cpp true)
add_test_file(src/transcoderoptionsopus_test.cpp true)
add_test_file(src/filesystemmusicstorage_test.cpp false)
add_test_file(src/player_test.cpp true)
add_test_file(src/deletefiles_test.cpp false)
add_test_file(src/deviceview_test.cpp false)
add_test_file(src/deviceproperties_test.cpp true)
add_test_file(src/devicemanager_test.cpp true)
add_test_file(src/concurrentrun_test.cpp false)
add_test_file(src/mergedproxymodel_test.cpp false)
add_test_file(src/settingsmigration_test.cpp false)
add_test_file(src/imageutils_test.cpp false)
add_test_file(src/sqlite_test.cpp false)
add_test_file(src/collectionsettingsdirectorymodel_test.cpp true)
add_test_file(src/gstenginepipeline_test.cpp false)
if(NOT ANDROID)
  add_test_file(src/sidebarsettingspage_test.cpp true)
  qt_wrap_cpp(SIDEBAR-TEST-MOC ${CMAKE_SOURCE_DIR}/src/settings/sidebarsettingspage_android.h)
  target_sources(sidebarsettingspage_test PRIVATE
    ${SIDEBAR-TEST-MOC}
    ${CMAKE_SOURCE_DIR}/src/widgets/fancytabwidget_android.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/androidwidgets.cpp
    ${CMAKE_SOURCE_DIR}/src/settings/sidebarsettingspage_android.cpp
  )
  add_test_file(src/androidwidgets_test.cpp true)
  target_sources(androidwidgets_test PRIVATE ${CMAKE_SOURCE_DIR}/src/widgets/androidwidgets.cpp)
  add_test_file(src/collectionlettergroups_test.cpp true)
  target_sources(collectionlettergroups_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/collection/collectionlettergroups_android.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/listscrolling_android.cpp
  )
  add_test_file(src/listscrolling_test.cpp true)
  target_sources(listscrolling_test PRIVATE ${CMAKE_SOURCE_DIR}/src/widgets/listscrolling_android.cpp)
  add_test_file(src/collectionselection_test.cpp true)
  qt_wrap_cpp(COLLECTION-SELECTION-TEST-MOC ${CMAKE_SOURCE_DIR}/src/collection/collectionselection_android.h)
  target_sources(collectionselection_test PRIVATE
    ${COLLECTION-SELECTION-TEST-MOC}
    ${CMAKE_SOURCE_DIR}/src/collection/collectionselection_android.cpp
    ${CMAKE_SOURCE_DIR}/src/collection/collectionlettergroups_android.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/listscrolling_android.cpp
  )
  add_test_file(src/menupage_test.cpp true)
  target_sources(menupage_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/widgets/menupage_android.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/androidwidgets.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/scrollarea_android.cpp
  )
  add_test_file(src/scrollarea_test.cpp true)
  target_sources(scrollarea_test PRIVATE ${CMAKE_SOURCE_DIR}/src/widgets/scrollarea_android.cpp)
  add_test_file(src/scantaskcontrols_test.cpp true)
  target_sources(scantaskcontrols_test PRIVATE ${CMAKE_SOURCE_DIR}/src/widgets/scantaskcontrols_android.cpp)
  if(ENABLE_NAS)
    add_test_file(src/nasfileengine_test.cpp false)
    target_sources(nasfileengine_test PRIVATE ${CMAKE_SOURCE_DIR}/src/engine/gstqfilesrc.cpp)
  endif()
  add_test_file(src/coverdialogpage_test.cpp true)
  target_sources(coverdialogpage_test PRIVATE ${CMAKE_SOURCE_DIR}/src/covermanager/coverdialogpage_android.cpp)
  add_test_file(src/collectiondialogpages_test.cpp true)
  target_sources(collectiondialogpages_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/collection/collectiondialogpages_android.cpp
    ${CMAKE_SOURCE_DIR}/src/covermanager/coverdialogpage_android.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/androidwidgets.cpp
    ${CMAKE_SOURCE_DIR}/src/widgets/scrollarea_android.cpp
  )
  add_test_file(src/gstaacdecoder_test.cpp false)
  target_sources(gstaacdecoder_test PRIVATE ${CMAKE_SOURCE_DIR}/src/engine/gstaacdecoder.cpp)
  add_test_file(src/spotifytrust_test.cpp false)
  target_sources(spotifytrust_test PRIVATE ${CMAKE_SOURCE_DIR}/src/engine/spotifytrust_android.cpp)
  add_test_file(src/alarmschedule_test.cpp false)
  target_sources(alarmschedule_test PRIVATE ${CMAKE_SOURCE_DIR}/src/alarm/alarmschedule_android.cpp)
  add_test_file(src/alarmtracks_test.cpp false)
  target_sources(alarmtracks_test PRIVATE ${CMAKE_SOURCE_DIR}/src/alarm/alarmtracks_android.cpp)
  add_test_file(src/alarmplayback_test.cpp false)
  target_sources(alarmplayback_test PRIVATE ${CMAKE_SOURCE_DIR}/src/alarm/alarmplayback_android.cpp)
  add_test_file(src/alarmtext_test.cpp false)
  target_sources(alarmtext_test PRIVATE ${CMAKE_SOURCE_DIR}/src/alarm/alarmtext_android.cpp ${CMAKE_SOURCE_DIR}/src/alarm/alarmschedule_android.cpp)
  add_test_file(src/timedialmath_test.cpp false)
  target_sources(timedialmath_test PRIVATE ${CMAKE_SOURCE_DIR}/src/widgets/timedialmath_android.cpp)
  # The Android alarm scheduling is Java; its test runs on a desktop JVM when a JDK is found.
  find_package(Java COMPONENTS Development)
  if(Java_FOUND)
    add_test(NAME alarmschedule_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/alarmschedule_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.AlarmScheduleTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/AlarmSchedule.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/AlarmScheduleTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
    add_test(NAME alarmfallback_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/alarmfallback_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.AlarmFallbackTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/AlarmFallback.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/AlarmFallbackTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
    add_test(NAME alarmpermissions_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/alarmpermissions_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.AlarmPermissionsTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/AlarmPermissions.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/AlarmPermissionsTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
    add_test(NAME alarmcountdown_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/alarmcountdown_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.AlarmCountdownTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/AlarmCountdown.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/AlarmCountdownTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
    add_test(NAME alarmhandover_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/alarmhandover_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.AlarmHandoverTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/AlarmHandover.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/AlarmHandoverTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
    add_test(NAME alarmrestart_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/alarmrestart_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.AlarmRestartTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/AlarmRestart.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/AlarmRestartTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
    add_test(NAME clockface_java_test COMMAND ${CMAKE_COMMAND}
      -DJAVAC=${Java_JAVAC_EXECUTABLE} -DJAVA=${Java_JAVA_EXECUTABLE}
      -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/clockface_java_test
      -DTEST_CLASS=nl.dewitweb.strawberryflavor.ClockFaceTest
      "-DSOURCES=${CMAKE_SOURCE_DIR}/android/java/nl/dewitweb/strawberryflavor/ClockFace.java|${CMAKE_CURRENT_SOURCE_DIR}/java/nl/dewitweb/strawberryflavor/ClockFaceTest.java"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/java/run_java_test.cmake)
  endif()
  add_test_file(src/androidstoragefileengine_test.cpp false)
  target_sources(androidstoragefileengine_test PRIVATE ${CMAKE_SOURCE_DIR}/src/core/androidstoragefileengine.cpp)
  target_link_libraries(androidstoragefileengine_test PRIVATE Qt${QT_VERSION_MAJOR}::CorePrivate)
  add_test_file(src/gstqfilesrc_test.cpp false)
  target_sources(gstqfilesrc_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/engine/gstqfilesrc.cpp
    ${CMAKE_SOURCE_DIR}/src/tagreader/taglibfilestream_android.cpp
  )
endif()
add_test_file(src/gstengine_test.cpp false)
add_test_file(src/gstpadutils_test.cpp false)
add_test_file(src/streammetadataparser_test.cpp false)
add_test_file(src/tagreader_test.cpp false)
add_test_file(src/tagreaderfilelocks_test.cpp false)
add_test_file(src/collectionbackend_test.cpp false)
add_test_file(src/collectionwatcher_test.cpp false)
add_test_file(src/collectionmodel_test.cpp true)
# The album cover thumbnails depend on the device pixel ratio, so they are also tested with a scaled display.
add_test(NAME collectionmodel_test_scale_factor_2 COMMAND ${CMAKE_CURRENT_BINARY_DIR}/collectionmodel_test${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*Thumbnail*)
set_tests_properties(collectionmodel_test_scale_factor_2 PROPERTIES ENVIRONMENT QT_SCALE_FACTOR=2)
add_test_file(src/collectionmanager_test.cpp true)
add_test_file(src/equalizer_test.cpp true)
add_test_file(src/playlist_test.cpp true)
add_test_file(src/playlistview_test.cpp true)
add_test_file(src/playlistmanager_test.cpp true)
add_test_file(src/playlistbackend_test.cpp false)
add_test_file(src/playlistlistmodel_test.cpp false)
add_test_file(src/playlisttabbar_test.cpp true)
add_test_file(src/songplaylistitem_test.cpp false)
add_test_file(src/songloader_test.cpp false)
add_test_file(src/smartplaylistsearch_test.cpp false)
add_test_file(src/smartplaylistsearchpreview_test.cpp true)
add_test_file(src/smartplaylistsearchtermwidget_test.cpp true)
add_test_file(src/playlistquerygenerator_test.cpp false)
add_test_file(src/filterparser_test.cpp false)
add_test_file(src/xspfparser_test.cpp false)
add_test_file(src/m3uparser_test.cpp false)
add_test_file(src/plsparser_test.cpp false)
add_test_file(src/asxparser_test.cpp false)
add_test_file(src/cueparser_test.cpp false)
add_test_file(src/playlistparsercue_test.cpp true)
add_test_file(src/organizeformat_test.cpp false)
add_test_file(src/organize_test.cpp false)
add_test_file(src/organizedialog_test.cpp true)
add_test_file(src/htmllyricsprovider_test.cpp false)
add_test_file(src/letraslyricsprovider_test.cpp false)
add_test_file(src/lyricsfetcher_test.cpp false)
add_test_file(src/lyricssearchdialog_test.cpp true)
add_test_file(src/songeditor_test.cpp true)
add_test_file(src/resizabletextedit_test.cpp true)
add_test_file(src/fancytabwidget_test.cpp true)
add_test_file(src/gridflowlayout_test.cpp true)
add_test_file(src/playlisttitleheader_test.cpp true)
add_test_file(src/longpresshandler_test.cpp true)
add_test_file(src/playlistactionhandler_test.cpp true)
add_test_file(src/actiondialog_test.cpp true)
add_test_file(src/equalizerjson_test.cpp false)
add_test_file(src/boomanalyzer_test.cpp true)
add_test_file(src/transcodedialog_test.cpp true)
add_test_file(src/trackslider_test.cpp true)
add_test_file(src/appearancesettingspage_test.cpp true)
add_test_file(src/collectionsettingspage_test.cpp true)
add_test_file(src/backendsettingspage_test.cpp true)
add_test_file(src/playlistsettingspage_test.cpp true)
add_test_file(src/contextsettingspage_test.cpp true)
add_test_file(src/fileview_test.cpp true)
add_test_file(src/trackselectiondialog_test.cpp true)
add_test_file(src/albumcoverexporter_test.cpp false)
add_test_file(src/albumcoverchoicecontroller_test.cpp true)
add_test_file(src/albumcoverloader_test.cpp true)
add_test_file(src/albumcoversearcher_test.cpp true)
add_test_file(src/groupediconview_test.cpp true)
add_test_file(src/albumcovermanager_test.cpp true)
add_test_file(src/coverfromurldialog_test.cpp true)
add_test_file(src/coverprovidercancel_test.cpp false)
add_test_file(src/scrobblercache_test.cpp false)
add_test_file(src/audioscrobbler_test.cpp false)
add_test_file(src/listenbrainzscrobbler_test.cpp false)
add_test_file(src/radioservices_test.cpp true)
add_test_file(src/radioview_test.cpp true)
add_test_file(src/errordialog_test.cpp true)
add_test_file(src/osdpretty_test.cpp true)
if(LINUX)
  add_test_file(src/filesystemwatcherinotify_test.cpp false)
endif()
if(HAVE_ALSA)
  add_test_file(src/alsaaudiodevicelister_test.cpp false)
endif()

if(HAVE_MPRIS2)
  add_test_file(src/mpris2_test.cpp false)
  target_link_libraries(mpris2_test PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
endif()
if(HAVE_MOODBAR)
  add_test_file(src/moodbarbuilder_test.cpp false)
  add_test_file(src/moodbarloader_test.cpp false)
  add_test_file(src/moodbarpipeline_test.cpp false)
  add_test_file(src/moodbarproxystyle_test.cpp true)
  add_test_file(src/moodbarrenderer_test.cpp true)
endif()
if(HAVE_WAVEFORM)
  add_test_file(src/waveformbuilder_test.cpp false)
  add_test_file(src/waveformpipeline_test.cpp false)
  add_test_file(src/waveformloader_test.cpp false)
  add_test_file(src/waveformcontroller_test.cpp false)
  add_test_file(src/waveformrenderer_test.cpp true)
  add_test_file(src/waveformproxystyle_test.cpp true)
endif()
if(HAVE_TAGFETCHER)
  add_test_file(src/acoustidclient_test.cpp false)
endif()
if(HAVE_GPOD)
  add_test_file(src/gpoddevice_test.cpp true)
  target_link_libraries(gpoddevice_test PRIVATE PkgConfig::LIBGPOD PkgConfig::GDK_PIXBUF)
endif()
if(HAVE_PLEX)
  add_test_file(src/plexrequest_test.cpp false)
endif()
if(HAVE_SUBSONIC)
  add_test_file(src/subsonicscrobbler_test.cpp false)
  add_test_file(src/subsonicbaserequest_test.cpp false)
  add_test_file(src/subsonicservice_test.cpp true)
endif()
if(HAVE_TIDAL OR HAVE_QOBUZ OR HAVE_SPOTIFY)
  add_test_file(src/streamingrequest_test.cpp true)
endif()
if(HAVE_SPOTIFY)
  add_test_file(src/spotifyfavoriterequest_test.cpp false)
  add_test_file(src/spotifyrequest_test.cpp true)
endif()
if(HAVE_DISCORD_RPC)
  add_test_file(src/discordrpc_test.cpp false)
endif()
if(MSVC)
  add_test_file(src/winsystemmediatransportcontrols_test.cpp false)
endif()
if(HAVE_GLOBALSHORTCUTS)
  add_test_file(src/globalshortcutgrabber_test.cpp true)
  add_test_file(src/globalshortcutsmanager_test.cpp true)
  add_test_file(src/globalshortcutssettingspage_test.cpp true)
endif()

add_custom_target(run_strawberry_tests COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS strawberry_tests)

# Live integration tests against real lyrics provider endpoints.
# NOT part of the default test run -- they hit the network and may fail when a provider's API changes or has an outage.
add_executable(lyrics_live_tests EXCLUDE_FROM_ALL src/lyricsproviders_live_test.cpp)
target_include_directories(lyrics_live_tests PRIVATE
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(lyrics_live_tests PRIVATE
  ${CMAKE_THREAD_LIBS_INIT}
  PkgConfig::GLIB
  PkgConfig::GOBJECT
  PkgConfig::GSTREAMER_BASE
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Sql
  Qt${QT_VERSION_MAJOR}::Test
  Qt${QT_VERSION_MAJOR}::Widgets
  test_utils
  test_main
)
