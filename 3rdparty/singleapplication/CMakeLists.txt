cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -U__STRICT_ANSI__ -Wall -Woverloaded-virtual -Wno-sign-compare -fpermissive")

set(SINGLEAPP-SOURCES singleapplication.cpp singleapplication_p.cpp)
set(SINGLEAPP-MOC-HEADERS singleapplication.h singleapplication_p.h)
QT5_WRAP_CPP(SINGLEAPP-SOURCES-MOC ${SINGLEAPP-MOC-HEADERS})
ADD_LIBRARY(singleapplication STATIC ${SINGLEAPP-SOURCES} ${SINGLEAPP-SOURCES-MOC})
target_link_libraries(singleapplication Qt5::Core Qt5::Widgets Qt5::Network)

set(SINGLECOREAPP-SOURCES singlecoreapplication.cpp singlecoreapplication_p.cpp)
set(SINGLECOREAPP-MOC-HEADERS singlecoreapplication.h singlecoreapplication_p.h)
QT5_WRAP_CPP(SINGLECOREAPP-SOURCES-MOC ${SINGLECOREAPP-MOC-HEADERS})
ADD_LIBRARY(singlecoreapplication STATIC ${SINGLECOREAPP-SOURCES} ${SINGLECOREAPP-SOURCES-MOC})
target_link_libraries(singlecoreapplication Qt5::Core Qt5::Widgets Qt5::Network)
