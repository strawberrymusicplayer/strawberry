if(LSB_RELEASE_EXEC AND DPKG_BUILDPACKAGE)

  execute_process(COMMAND env LC_ALL=C date "+%a, %-d %b %Y %H:%M:%S %z" OUTPUT_VARIABLE DEB_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND /bin/sh "-c" "${LSB_RELEASE_EXEC} -cs" OUTPUT_VARIABLE DEB_CODENAME OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(DEB_CODENAME AND DEB_DATE)

    if(QT_VERSION_MAJOR EQUAL 5)
      set(DEBIAN_BUILD_DEPENDS_QT_PACKAGES qtbase5-dev,qtbase5-dev-tools,qttools5-dev,qttools5-dev-tools,libqt5x11extras5-dev)
      set(DEBIAN_DEPENDS_QT_PACKAGES libqt5sql5-sqlite)
    endif()
    if(QT_VERSION_MAJOR EQUAL 6)
      set(DEBIAN_BUILD_DEPENDS_QT_PACKAGES qt6-base-dev,qt6-base-dev-tools,qt6-tools-dev,qt6-tools-dev-tools,qt6-l10n-tools)
      set(DEBIAN_DEPENDS_QT_PACKAGES libqt6sql6-sqlite,qt6-qpa-plugins)
    endif()

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/control.in ${CMAKE_CURRENT_SOURCE_DIR}/control @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/changelog.in ${CMAKE_CURRENT_SOURCE_DIR}/changelog)
  
  endif()

endif()
