cmake_minimum_required(VERSION 2.8.11)
set(CMAKE_CXX_STANDARD 11)

include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
if (NOT WIN32 AND NOT APPLE)
  find_path(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H qpa/qplatformnativeinterface.h PATHS ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
  if(NOT HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
    message(FATAL_ERROR "Missing qpa/qplatformnativeinterface.h, check that you got the QT private headers installed (package libQt5Gui-private-headers-devel, qtbase5-private-dev or similar)")
  endif(NOT HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
endif(NOT WIN32 AND NOT APPLE)

set(QXT-SOURCES
  qxtglobal.cpp
  qxtglobalshortcut.cpp
)

set(QXT-MOC-HEADERS
  qxtglobalshortcut.h
)

find_package(X11)
include_directories(${X11_INCLUDE_DIR})

if(WIN32)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_win.cpp)
elseif(APPLE)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_mac.cpp)
else(WIN32)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_x11.cpp)
endif(WIN32)

QT5_WRAP_CPP(QXT-SOURCES-MOC ${QXT-MOC-HEADERS})

ADD_LIBRARY(qxt STATIC
  ${QXT-SOURCES}
  ${QXT-SOURCES-MOC}
)

if(WIN32)
  target_link_libraries(qxt Qt5::Core Qt5::Widgets)
else(WIN32)
  target_link_libraries(qxt Qt5::Core Qt5::Widgets Qt5::X11Extras)
endif(WIN32)
