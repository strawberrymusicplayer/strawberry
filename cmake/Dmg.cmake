#find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt PATHS /usr/local/opt/qt6/bin /usr/local/opt/qt5/bin /usr/local/bin REQUIRED)
set(MACDEPLOYQT_EXECUTABLE "${CMAKE_BINARY_DIR}/3rdparty/macdeployqt/macdeployqt")
if(MACDEPLOYQT_EXECUTABLE)
  message(STATUS "Found macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
else()
  message(WARNING "Missing macdeployqt executable.")
endif()

find_program(CREATEDMG_EXECUTABLE NAMES create-dmg REQUIRED)
if(CREATEDMG_EXECUTABLE)
  message(STATUS "Found create-dmg: ${CREATEDMG_EXECUTABLE}")
else()
  message(WARNING "Missing create-dmg executable.")
endif()

if(MACDEPLOYQT_EXECUTABLE)
  add_custom_target(copy_gstreamer_plugins
    #COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macgstcopy.sh strawberry.app
  )
  add_custom_target(deploy
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/strawberry.app/Contents/{Frameworks,Resources}
    COMMAND cp -v ${CMAKE_SOURCE_DIR}/dist/macos/Info.plist ${CMAKE_BINARY_DIR}/strawberry.app/Contents/
    COMMAND cp -v ${CMAKE_SOURCE_DIR}/dist/macos/strawberry.icns ${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources/
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -verbose=3
      -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/strawberry-tagreader
      -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/gio-modules/libgiognutls.so
      #-executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/gst-plugin-scanner
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS strawberry strawberry-tagreader copy_gstreamer_plugins macdeployqt
  )
  add_custom_target(deploycheck
    COMMAND ${CMAKE_BINARY_DIR}/ext/macdeploycheck/macdeploycheck strawberry.app
    DEPENDS macdeploycheck
  )
  if(CREATEDMG_EXECUTABLE)
    add_custom_target(dmg
      COMMAND ${CREATEDMG_EXECUTABLE} --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS deploy deploycheck
    )
  endif()
endif()
