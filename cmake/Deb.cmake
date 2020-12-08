find_program(LSB_RELEASE_EXEC lsb_release)
find_program(DPKG_BUILDPACKAGE dpkg-buildpackage)

if (LSB_RELEASE_EXEC AND DPKG_BUILDPACKAGE)
  execute_process(COMMAND env LC_ALL=C date "+%a, %-d %b %Y %H:%M:%S %z" OUTPUT_VARIABLE DEB_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND /bin/sh "-c" "${LSB_RELEASE_EXEC} -cs"
    OUTPUT_VARIABLE DEB_CODENAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  if (DEB_CODENAME AND DEB_DATE)
    add_custom_target(deb
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMAND ${DPKG_BUILDPACKAGE} -b -d -uc -us
    )
  endif()

endif()
