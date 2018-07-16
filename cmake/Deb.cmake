add_custom_target(deb
  #COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/dist/debian ${CMAKE_BINARY_DIR}/debian
  COMMAND cp -r -p ${CMAKE_SOURCE_DIR}/dist/debian ${CMAKE_BINARY_DIR}/
  COMMAND dpkg-buildpackage -b -d -uc -us
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
