#-------------------------------------------------
#
# Project created by QtCreator 2016-09-09T13:44:06
#
#-------------------------------------------------

QT       -= gui
QT       += sql network core
win32:  QT+= winextras
macx:   QT+= macextras
TARGET = scout-core
TEMPLATE = lib
CONFIG += staticlib c++11
CONFIG += debug_and_release
DEFINES += LIB_LIBRARY
QMAKE_MAC_SDK = macosx10.11

DEFINES += SCOUTCORE_LIBRARY

unix {
    target.path = /usr/lib
    INSTALLS += target
}

INCLUDEPATH += $$PWD/../sx-api
DEPENDPATH += $$PWD/../sx-api

win32 {
    HEADERS += \
        winstorage.h
    SOURCES += \
        winstorage.cpp
}
macx {
    LIBS += \
        "-framework CoreFoundation" \
        "-framework Cocoa"
}

HEADERS += \
    scoutqueue.h \
    scoutmodel.h \
    scoutmimedata.h \
    scoutdatabase.h

SOURCES += \
    scoutqueue.cpp \
    scoutmodel.cpp \
    scoutmimedata.cpp \
    scoutdatabase.cpp
