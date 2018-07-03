set(ENV{LC_ALL} "en_US.utf8")
execute_process(COMMAND date "+%a %b %d %Y" OUTPUT_VARIABLE RPM_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/rpm/strawberry.spec.in ${CMAKE_CURRENT_SOURCE_DIR}/rpm/strawberry.spec @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/scripts/maketarball.sh.in ${CMAKE_CURRENT_SOURCE_DIR}/scripts/maketarball.sh @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist)

if (UNIX AND NOT APPLE)
  install(FILES ../data/icons/48x48/strawberry.png DESTINATION share/icons/hicolor/48x48/apps/)
  install(FILES ../data/icons/64x64/strawberry.png DESTINATION share/icons/hicolor/64x64/apps/)
  install(FILES ../data/icons/128x128/strawberry.png DESTINATION share/icons/hicolor/128x128/apps/)
  install(FILES ../data/icons/128x128/strawberry.svg DESTINATION share/icons/hicolor/scalable/apps/)
  install(FILES unix/strawberry.desktop DESTINATION share/applications)
  install(FILES man/strawberry.1 man/strawberry-tagreader.1 DESTINATION share/man/man1)
endif()

if (APPLE)
  install(FILES macos/strawberry.icns DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources")
endif()
