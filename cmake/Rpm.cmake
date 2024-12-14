find_program(LSB_RELEASE_EXEC lsb_release)
find_program(RPMBUILD_EXEC rpmbuild)

if (LSB_RELEASE_EXEC AND RPMBUILD_EXEC)
  execute_process(COMMAND env LC_ALL="en_US.utf8" date "+%a %b %d %Y" OUTPUT_VARIABLE RPM_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

  execute_process(COMMAND /bin/sh "-c" "${LSB_RELEASE_EXEC} -is | tr '[:upper:]' '[:lower:]' | cut -d' ' -f1"
    OUTPUT_VARIABLE DIST_NAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  execute_process(COMMAND /bin/sh "-c" "${LSB_RELEASE_EXEC} -ds | tr '[:upper:]' '[:lower:]' | sed 's/\"//g' | cut -d' ' -f2"
    OUTPUT_VARIABLE DIST_RELEASE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if (${DIST_NAME} STREQUAL "openmandrivalinux")
    execute_process(COMMAND /bin/sh "-c" "${LSB_RELEASE_EXEC} -ds | tr '[:upper:]' '[:lower:]' | sed 's/\"//g' | sed 's/\\./0/g' | cut -d' ' -f3"
      OUTPUT_VARIABLE DIST_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  else()
    execute_process(COMMAND /bin/sh "-c" "${LSB_RELEASE_EXEC} -ds | tr '[:upper:]' '[:lower:]' | sed 's/\"//g' | sed 's/\\.//g' | cut -d' ' -f3"
      OUTPUT_VARIABLE DIST_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
  if (DIST_NAME)

    message(STATUS "Distro Name: ${DIST_NAME}")
    if (DIST_RELEASE)
      message(STATUS "Distro Release: ${DIST_RELEASE}")
    endif()
    if (DIST_VERSION)
      message(STATUS "Distro Version: ${DIST_VERSION}")
    endif()

    set(RPMBUILD_DIR ~/rpmbuild CACHE STRING "Rpmbuild directory, for the rpm target")

    if (${DIST_NAME} STREQUAL "opensuse" AND DIST_RELEASE)
      if (${DIST_RELEASE} STREQUAL "leap")
        if (DIST_VERSION)
          set(RPM_DISTRO "lp${DIST_VERSION}")
        else()
          set(RPM_DISTRO ${DIST_RELEASE})
        endif()
      elseif (${DIST_RELEASE} STREQUAL "tumbleweed")
        set(RPM_DISTRO ${DIST_RELEASE})
      endif()
    elseif (${DIST_NAME} STREQUAL "fedora" AND DIST_VERSION)
      set(RPM_DISTRO "fc${DIST_VERSION}")
    elseif (${DIST_NAME} STREQUAL "centos" AND DIST_VERSION)
      set(RPM_DISTRO "el${DIST_VERSION}")
    elseif (${DIST_NAME} STREQUAL "mageia" AND DIST_RELEASE)
      set(RPM_DISTRO "mga${DIST_RELEASE}")
    elseif (${DIST_NAME} STREQUAL "openmandrivalinux" AND DIST_VERSION)
      set(RPM_DISTRO "omv${DIST_VERSION}")
    endif()

    if(NOT RPM_DISTRO)
      set(RPM_DISTRO ${DIST_NAME})
    endif()

    message(STATUS "RPM Suffix: ${RPM_DISTRO}")

    add_custom_target(rpm
      COMMAND ${CMAKE_SOURCE_DIR}/dist/scripts/maketarball.sh
      COMMAND ${CMAKE_COMMAND} -E copy strawberry-${STRAWBERRY_VERSION_PACKAGE}.tar.xz ${RPMBUILD_DIR}/SOURCES/
      COMMAND ${RPMBUILD_EXEC} -ba ${CMAKE_BINARY_DIR}/strawberry.spec
    )

  endif()

endif()
