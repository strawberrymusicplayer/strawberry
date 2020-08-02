add_custom_target(dmg
  COMMAND /usr/local/opt/qt5/bin/macdeployqt strawberry.app
  # macdeploy.py is not picking up libicudata.dylib so manually copy it for now.
  COMMAND cp "/usr/local/opt/icu4c/lib/libicudata*.dylib" "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks"
  COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macdeploy.py strawberry.app
  COMMAND create-dmg --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}.dmg strawberry.app
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
