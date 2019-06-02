QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES +=  \
    fht.cpp \
    testfht.cpp

HEADERS += fht.h \
    testfht.h
