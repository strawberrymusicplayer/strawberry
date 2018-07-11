find_program(LSB_RELEASE_EXEC lsb_release)
execute_process(COMMAND ${LSB_RELEASE_EXEC} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (LSB_RELEASE_EXEC)
  set(RPMBUILD_DIR ~/rpmbuild CACHE STRING "Rpmbuild directory, for the rpm target")
  set(RPM_ARCH x86_64 CACHE STRING "Architecture of the rpm file")
  if (${LSB_RELEASE_ID_SHORT} STREQUAL "openSUSE")
    set(RPM_DISTRO opensuse CACHE STRING "Suffix of the rpm file")
    add_custom_target(rpm
      COMMAND ${CMAKE_SOURCE_DIR}/dist/scripts/maketarball.sh
      COMMAND ${CMAKE_COMMAND} -E copy strawberry-${STRAWBERRY_VERSION_PACKAGE}.tar.xz ${RPMBUILD_DIR}/SOURCES/
      COMMAND rpmbuild -bs ${CMAKE_SOURCE_DIR}/dist/opensuse/strawberry.spec
      COMMAND rpmbuild -bb ${CMAKE_SOURCE_DIR}/dist/opensuse/strawberry.spec
    )
  endif()
  if (${LSB_RELEASE_ID_SHORT} STREQUAL "Fedora")
    set(RPM_DISTRO fedora CACHE STRING "Suffix of the rpm file")
    add_custom_target(rpm
      COMMAND ${CMAKE_SOURCE_DIR}/dist/scripts/maketarball.sh
      COMMAND ${CMAKE_COMMAND} -E copy strawberry-${STRAWBERRY_VERSION_PACKAGE}.tar.xz ${RPMBUILD_DIR}/SOURCES/
      COMMAND rpmbuild -bs ${CMAKE_SOURCE_DIR}/dist/fedora/strawberry.spec
      COMMAND rpmbuild -bb ${CMAKE_SOURCE_DIR}/dist/fedora/strawberry.spec
    )
  endif()
endif()
