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

# Workaround for Sparkle
add_custom_target(copy_sparkle
  COMMAND mkdir -p ${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/
  COMMAND cp -r /usr/local/opt/sparkle/Sparkle.framework ${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  DEPENDS strawberry strawberry-tagreader
)

if(MACDEPLOYQT_EXECUTABLE)
  add_custom_target(deploy
    COMMAND mkdir -p ${CMAKE_BINARY_DIR}/strawberry.app/Contents/{Frameworks,Resources}
    COMMAND cp ${CMAKE_SOURCE_DIR}/dist/macos/Info.plist ${CMAKE_BINARY_DIR}/strawberry.app/Contents/
    COMMAND cp ${CMAKE_SOURCE_DIR}/dist/macos/strawberry.icns ${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources/
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -verbose=3 -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/strawberry-tagreader
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS strawberry strawberry-tagreader macdeployqt
  )
  if(CREATEDMG_EXECUTABLE)
    add_custom_target(dmg
      COMMAND ${CREATEDMG_EXECUTABLE} --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      DEPENDS deploy
    )
  endif()
endif()
