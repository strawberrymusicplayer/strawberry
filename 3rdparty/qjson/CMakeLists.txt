cmake_minimum_required(VERSION 2.8.11)

set(QJSON-SOURCES
    src/parser.cpp
    src/qobjecthelper.cpp
    src/json_scanner.cpp
    src/json_parser.cc
    src/parserrunnable.cpp
    src/serializer.cpp
    src/serializerrunnable.cpp
)

set(QJSON-MOC-HEADERS
    src/parser.h
    src/parserrunnable.h
    src/qobjecthelper.h
    src/serializer.h
    src/serializerrunnable.h
    src/qjson_export.h
)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

QT5_WRAP_CPP(QJSON-SOURCES-MOC ${QJSON-MOC-HEADERS})

add_library(qjson STATIC
   ${QJSON-SOURCES}
   ${QJSON-SOURCES-MOC}
   ${QJSON-WIN32-RESOURCES}
)

set_property(TARGET qjson PROPERTY QT_STATICPLUGIN 1)

target_link_libraries(qjson
    Qt5::Core
    ${QJSON_LIBRARIES}
)
