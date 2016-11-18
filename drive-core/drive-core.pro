#-------------------------------------------------
#
# Project created by QtCreator 2016-02-12T10:46:56
#
#-------------------------------------------------

QT       -= gui
QT       += sql network core

TARGET = drive-core
TEMPLATE = lib
CONFIG += staticlib c++11
CONFIG += debug_and_release
DEFINES += LIB_LIBRARY
QMAKE_MAC_SDK = macosx10.11

SOURCES += sxconfig.cpp \
    sxdatabase.cpp \
    sxcontroller.cpp \
    sxqueue.cpp \
    sxfilesystem.cpp \
    sxstate.cpp \
    sxvolumeentry.cpp \
    uploadqueue.cpp

HEADERS += sxconfig.h \
    sxdatabase.h \
    sxcontroller.h \
    sxqueue.h \
    sxfilesystem.h \
    sxstate.h \
    sxvolumeentry.h \
    uploadqueue.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

unix:!macx: MAKE_DEB=$$(SXDRIVE_MAKEDEB)
isEqual(MAKE_DEB, "yes"): DEFINES += "MAKE_DEB"

INCLUDEPATH += $$PWD/../sx-api
DEPENDPATH += $$PWD/../sx-api
