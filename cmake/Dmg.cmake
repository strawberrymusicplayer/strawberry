if(BUILD_WITH_QT6)
  set(MACDEPLOYQT_PATHS "/usr/local/opt/qt6/bin")
elseif(BUILD_WITH_QT5)
  set(MACDEPLOYQT_PATHS "/usr/local/opt/qt5/bin")
else()
  message(FATAL_ERROR "BUILD_WITH_QT6 or BUILD_WITH_QT5 must be set.")
endif()

find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt PATHS ${MACDEPLOYQT_PATHS} NO_DEFAULT_PATH)
if(NOT MACDEPLOYQT_EXECUTABLE)
  message(WARNING "Missing macdeployqt executable.")
endif()

execute_process(COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macversion.sh OUTPUT_VARIABLE MACOS_VERSION_PACKAGE OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT MACOS_VERSION_PACKAGE)
  message(WARNING "Could not set macOS version.")
endif()

if(MACDEPLOYQT_EXECUTABLE AND MACOS_VERSION_PACKAGE)
  add_custom_target(dmg
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/strawberry-tagreader
    COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macdeploy.py strawberry.app
    COMMAND create-dmg --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}-${MACOS_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  add_custom_target(dmg2
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/strawberry-tagreader
    COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macdeploy.py strawberry.app
    COMMAND create-dmg --skip-jenkins --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}-${MACOS_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()
