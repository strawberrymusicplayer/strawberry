find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt PATHS /usr/bin /usr/local/bin /opt/local/bin /usr/local/opt/qt6/bin REQUIRED)
if(MACDEPLOYQT_EXECUTABLE)
  message(STATUS "Found macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
else()
  message(WARNING "Missing macdeployqt executable.")
endif()

find_program(MACDEPLOYCHECK_EXECUTABLE NAMES macdeploycheck PATHS /usr/bin /usr/local/bin /opt/local/bin /usr/local/opt/qt6/bin REQUIRED)
if(MACDEPLOYCHECK_EXECUTABLE)
  message(STATUS "Found macdeploycheck: ${MACDEPLOYCHECK_EXECUTABLE}")
else()
  message(WARNING "Missing macdeploycheck executable.")
endif()

find_program(CREATEDMG_EXECUTABLE NAMES create-dmg REQUIRED)
if(CREATEDMG_EXECUTABLE)
  message(STATUS "Found create-dmg: ${CREATEDMG_EXECUTABLE}")
else()
  message(WARNING "Missing create-dmg executable.")
endif()

if(MACDEPLOYQT_EXECUTABLE)

  if(APPLE_DEVELOPER_ID)
    set(MACDEPLOYQT_CODESIGN -codesign=${APPLE_DEVELOPER_ID})
    set(CREATEDMG_CODESIGN --codesign ${APPLE_DEVELOPER_ID})
  endif()
  if(CREATEDMG_SKIP_JENKINS)
    set(CREATEDMG_SKIP_JENKINS_ARG "--skip-jenkins")
  endif()

  if(APPLE_DEVELOPER_ID)
    set(CODESIGN_IDENTITY ${APPLE_DEVELOPER_ID})
  else()
    set(CODESIGN_IDENTITY "-")
  endif()

  add_custom_target(deploy
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/strawberry.app/Contents/{Frameworks,Resources}
    COMMAND cp -v ${CMAKE_BINARY_DIR}/dist/macos/Info.plist ${CMAKE_BINARY_DIR}/strawberry.app/Contents/
    COMMAND cp -v ${CMAKE_SOURCE_DIR}/dist/macos/strawberry.icns ${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources/
    COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macgstcopy.sh ${CMAKE_BINARY_DIR}/strawberry.app
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -verbose=3 -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/gst-plugin-scanner ${MACDEPLOYQT_CODESIGN}
    # macdeployqt leaves some libraries with invalidated signatures: the libsoup copied by macgstcopy.sh (its dependencies get rewritten afterwards) and the dependencies macdeployqt pulls in for it (libsqlite3, libpsl, libnghttp2).
    # A library with an invalid signature fails to dlopen on Apple Silicon, silently breaking the soup plugin (HTTP/HTTPS streaming), so re-sign everything and then the bundle itself.
    COMMAND sh -c "codesign --force --sign '${CODESIGN_IDENTITY}' ${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/*.dylib"
    COMMAND sh -c "codesign --force --sign '${CODESIGN_IDENTITY}' ${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/gstreamer/*.dylib ${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/gio-modules/*.so ${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/gst-plugin-scanner"
    COMMAND codesign --force --sign ${CODESIGN_IDENTITY} ${CMAKE_BINARY_DIR}/strawberry.app
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS strawberry
  )
  if(MACDEPLOYCHECK_EXECUTABLE)
    add_custom_target(deploycheck
      COMMAND ${MACDEPLOYCHECK_EXECUTABLE} strawberry.app
    )
  endif()
  if(CREATEDMG_EXECUTABLE)
    add_custom_target(dmg
      COMMAND ${CREATEDMG_EXECUTABLE} --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 ${CREATEDMG_CODESIGN} ${CREATEDMG_SKIP_JENKINS_ARG} strawberry-${STRAWBERRY_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
  endif()
endif()
