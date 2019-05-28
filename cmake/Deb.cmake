add_custom_target(deb
  COMMAND cp -r -p ${CMAKE_SOURCE_DIR}/debian ${CMAKE_BINARY_DIR}/
  COMMAND dpkg-buildpackage -b -d -uc -us
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
