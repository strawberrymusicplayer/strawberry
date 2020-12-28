find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt PATHS /usr/local/opt/qt6/bin /usr/local/opt/qt5/bin /usr/local/bin)
if(MACDEPLOYQT_EXECUTABLE)
  message(STATUS "Found macdeployqt: ${MACDEPLOYQT_EXECUTABLE}")
else()
  message(WARNING "Missing macdeployqt executable.")
endif()

find_program(CREATEDMG_EXECUTABLE NAMES create-dmg)
if(CREATEDMG_EXECUTABLE)
  message(STATUS "Found create-dmg: ${CREATEDMG_EXECUTABLE}")
else()
  message(WARNING "Missing create-dmg executable.")
endif()

execute_process(COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macversion.sh OUTPUT_VARIABLE MACOS_VERSION_PACKAGE OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT MACOS_VERSION_PACKAGE)
  message(WARNING "Could not set macOS version.")
endif()

if(MACDEPLOYQT_EXECUTABLE AND CREATEDMG_EXECUTABLE AND MACOS_VERSION_PACKAGE)
  add_custom_target(dmg
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/strawberry-tagreader
    COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macdeploy.py strawberry.app
    COMMAND ${CREATEDMG_EXECUTABLE} --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}-${MACOS_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  add_custom_target(dmg2
    COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -executable=${CMAKE_BINARY_DIR}/strawberry.app/Contents/PlugIns/strawberry-tagreader
    COMMAND ${CMAKE_SOURCE_DIR}/dist/macos/macdeploy.py strawberry.app
    COMMAND ${CREATEDMG_EXECUTABLE} --skip-jenkins --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 strawberry-${STRAWBERRY_VERSION_PACKAGE}-${MACOS_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
endif()
