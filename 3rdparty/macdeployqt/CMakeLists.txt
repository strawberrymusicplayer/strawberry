add_executable(macdeployqt main.cpp shared.cpp)
target_link_libraries(macdeployqt PRIVATE
  "-framework AppKit"
  Qt${QT_VERSION_MAJOR}::Core
)

#execute_process(COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/macdeployqt ${CMAKE_BINARY_DIR})
