enable_testing()
find_package(CppUnit REQUIRED)

SET(TAGLIB_TESTS_SOURCES
  main.cpp
  test_list.cpp
  test_map.cpp
  test_mpeg.cpp
  test_synchdata.cpp
  test_trueaudio.cpp
  test_bytevector.cpp
  test_bytevectorlist.cpp
  test_bytevectorstream.cpp
  test_string.cpp
  test_propertymap.cpp
  test_file.cpp
  test_fileref.cpp
  test_id3v1.cpp
  test_id3v2.cpp
  test_xiphcomment.cpp
  test_aiff.cpp
  test_riff.cpp
  test_ogg.cpp
  test_oggflac.cpp
  test_flac.cpp
  test_flacpicture.cpp
  test_flacunknownmetadatablock.cpp
  test_ape.cpp
  test_apetag.cpp
  test_wav.cpp
  test_info.cpp
  test_wavpack.cpp
  test_mp4.cpp
  test_mp4item.cpp
  test_mp4coverart.cpp
  test_asf.cpp
  test_mod.cpp
  test_s3m.cpp
  test_it.cpp
  test_xm.cpp
  test_mpc.cpp
  test_opus.cpp
  test_speex.cpp
  test_dsf.cpp
  test_dsdiff.cpp
)

link_directories(${TAGLIB_LIBRARY_DIRS})

add_executable(taglib_tests ${TAGLIB_TESTS_SOURCES})

target_include_directories(taglib_tests PRIVATE
  ${CPPUNIT_INCLUDE_DIR}
  ${TAGLIB_INCLUDE_DIRS}
  ${CMAKE_CURRENT_BINARY_DIR}/../../3rdparty/taglib
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/toolkit
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/ape
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/asf
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mpeg/id3v1
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mpeg/id3v2
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mpeg/id3v2/frames
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mpeg
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mpc
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mp4
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/riff
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/riff/aiff
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/riff/wav
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/trueaudio
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/ogg
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/ogg/vorbis
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/ogg/flac
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/ogg/speex
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/ogg/opus
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/flac
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/wavpack
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/mod
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/s3m
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/it
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/xm
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/dsf
  ${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/taglib/dsdiff
)

target_link_libraries(taglib_tests PRIVATE
  ${CPPUNIT_LIBRARIES}
  ${TAGLIB_LIBRARIES}
)

add_test(taglib_tests taglib_tests)
add_custom_target(run_taglib_tests COMMAND ${CMAKE_CTEST_COMMAND} -V DEPENDS taglib_tests)
