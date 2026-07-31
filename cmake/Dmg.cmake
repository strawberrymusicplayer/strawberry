find_program(MACDEPLOYQT_EXECUTABLE NAMES macdeployqt PATHS /usr/bin /usr/local/bin /opt/local/bin /usr/local/opt/qt6/bin REQUIRED)
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
  set(DEPLOY_COMMANDS
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component macos_bundle_resources
  )
  if(APPLE_DEVELOPER_ID)
    list(APPEND DEPLOY_COMMANDS COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -verbose=3 -codesign=${APPLE_DEVELOPER_ID} -gstreamer-plugins=all)
  else()
    list(APPEND DEPLOY_COMMANDS COMMAND ${MACDEPLOYQT_EXECUTABLE} strawberry.app -verbose=3 -no-codesign -gstreamer-plugins=all)
  endif()
  add_custom_target(deploy ${DEPLOY_COMMANDS} WORKING_DIRECTORY ${CMAKE_BINARY_DIR} DEPENDS strawberry)
  if(CREATEDMG_EXECUTABLE)
    if(APPLE_DEVELOPER_ID)
      set(CREATEDMG_CODESIGN --codesign ${APPLE_DEVELOPER_ID})
    endif()
    if(CREATEDMG_SKIP_JENKINS)
      set(CREATEDMG_SKIP_JENKINS_ARG "--skip-jenkins")
    endif()
    add_custom_target(dmg
      COMMAND ${CREATEDMG_EXECUTABLE} --volname strawberry --background "${CMAKE_SOURCE_DIR}/dist/macos/dmg_background.png" --app-drop-link 450 218 --icon strawberry.app 150 218 --window-size 600 450 ${CREATEDMG_CODESIGN} ${CREATEDMG_SKIP_JENKINS_ARG} strawberry-${STRAWBERRY_VERSION_PACKAGE}-${CMAKE_HOST_SYSTEM_PROCESSOR}.dmg strawberry.app
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
  endif()
endif()
