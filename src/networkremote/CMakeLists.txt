if(HAVE_NETWORKREMOTE)
  set(SOURCES RemoteMessages.proto)

  link_directories(
    ${GLIB_LIBRARY_DIRS}
    ${PROTOBUF_LIBRARY_DIRS}
  )

  add_library(lib-networkremote OBJECT ${PROTO_SOURCES} ${SOURCES})

  target_include_directories(lib-networkremote SYSTEM PRIVATE
    ${GLIB_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
  )

  target_include_directories(lib-networkremote PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/src
  )

  target_link_libraries(lib-networkremote PRIVATE
    ${GLIB_LIBRARIES}
    ${Protobuf_LIBRARIES}
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
  )

protobuf_generate(TARGET lib-networkremote)

endif()
