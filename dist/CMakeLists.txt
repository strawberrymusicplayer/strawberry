execute_process(COMMAND date "+%a %b %d %Y"
                OUTPUT_VARIABLE RPM_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/strawberry.spec.in
               ${CMAKE_CURRENT_SOURCE_DIR}/strawberry.spec @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/maketarball.sh.in
               ${CMAKE_CURRENT_SOURCE_DIR}/maketarball.sh @ONLY)

if (UNIX)
  install(FILES ../data/icons/48x48/strawberry.png
    DESTINATION share/icons/hicolor/48x48/apps/
  )
  install(FILES ../data/icons/64x64/strawberry.png
    DESTINATION share/icons/hicolor/64x64/apps/
  )
  install(FILES ../data/icons/128x128/strawberry.png
    DESTINATION share/icons/hicolor/128x128/apps/
  )
  install(FILES ../data/icons/128x128/strawberry.svg
    DESTINATION share/icons/hicolor/scalable/apps/
  )
  install(FILES strawberry.desktop
    DESTINATION share/applications
  )
  install(FILES strawberry.1 strawberry-tagreader.1
    DESTINATION share/man/man1
  )
endif (UNIX)
