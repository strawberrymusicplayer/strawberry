add_custom_target(dmg
  COMMAND sudo ${CMAKE_SOURCE_DIR}/dist/macos/macdeploy.py strawberry.app
  COMMAND sudo ${CMAKE_SOURCE_DIR}/dist/macos/create-dmg.sh strawberry.app
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
