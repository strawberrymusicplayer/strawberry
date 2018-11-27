cmake_minimum_required(VERSION 2.8.11)
set(CMAKE_CXX_STANDARD 11)

if (UNIX AND NOT APPLE)
  find_package(X11)
  if (X11_FOUND)
    include_directories(${X11_INCLUDE_DIR})
  endif(X11_FOUND)
endif(UNIX AND NOT APPLE)

include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
if (NOT WIN32 AND NOT APPLE)
  find_path(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H qpa/qplatformnativeinterface.h PATHS ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
  if(NOT HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
    message(FATAL_ERROR "Missing qpa/qplatformnativeinterface.h, check that you got the QT private headers installed (package libQt5Gui-private-headers-devel, qtbase5-private-dev or similar)")
  endif(NOT HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
endif(NOT WIN32 AND NOT APPLE)

set(QXT-SOURCES qxtglobal.cpp qxtglobalshortcut.cpp)
set(QXT-MOC-HEADERS qxtglobalshortcut.h )

if(X11_FOUND)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_x11.cpp)
elseif(APPLE)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_mac.cpp)
elseif(WIN32)
  set(QXT-SOURCES ${QXT-SOURCES} qxtglobalshortcut_win.cpp)
endif()

QT5_WRAP_CPP(QXT-SOURCES-MOC ${QXT-MOC-HEADERS})

ADD_LIBRARY(qxt STATIC
  ${QXT-SOURCES}
  ${QXT-SOURCES-MOC}
)

target_link_libraries(qxt Qt5::Core Qt5::Widgets)

if(X11_FOUND)
  target_link_libraries(qxt Qt5::X11Extras)
endif(X11_FOUND)
