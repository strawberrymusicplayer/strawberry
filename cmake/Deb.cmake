find_program(LSB_RELEASE_EXEC lsb_release)
find_program(DPKG_BUILDPACKAGE dpkg-buildpackage)

if (LSB_RELEASE_EXEC AND DPKG_BUILDPACKAGE)
  add_custom_target(deb WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} COMMAND ${DPKG_BUILDPACKAGE} -b -d -uc -us -nc -j4)
endif()
