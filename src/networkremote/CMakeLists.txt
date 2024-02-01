cmake_minimum_required(VERSION 3.14)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_subdirectory(proto)

#target_link_libraries(MyProtoBuffTest Qt${QT_VERSION_MAJOR}::Core)
#target_include_directories(MyProtoBuffTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")


#include(GNUInstallDirs)
#install(TARGETS MyProtoBuffTest
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}

