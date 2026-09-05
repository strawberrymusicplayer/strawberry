file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib-networkremote_autogen/include)

add_library(lib-networkremote STATIC)

qt_add_protobuf(lib-networkremote
    PROTO_FILES RemoteMessages.proto
)

target_include_directories(lib-networkremote SYSTEM PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
)

target_link_libraries(lib-networkremote PUBLIC
  Qt${QT_VERSION_MAJOR}::Protobuf
)

target_link_libraries(lib-networkremote PRIVATE
  ${GLIB_LIBRARIES}
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Gui
)