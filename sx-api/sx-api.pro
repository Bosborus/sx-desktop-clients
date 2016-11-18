#-------------------------------------------------
#
# Project created by QtCreator 2016-01-19T13:10:04
#
#-------------------------------------------------

QT += network core
QT -= gui

TARGET = sx-api
TEMPLATE = lib
CONFIG += staticlib c++11
CONFIG += debug_and_release
DEFINES += LIB_LIBRARY
QMAKE_MAC_SDK = macosx10.11

SOURCES += \
    sxcluster.cpp \
    sxquery.cpp \
    sxauth.cpp \
    sxqueryresult.cpp \
    sxmeta.cpp \
    sxuserinfo.cpp \
    sxvolume.cpp \
    sxfile.cpp \
    sxfileentry.cpp \
    sxblock.cpp \
    sxjob.cpp \
    sxfilter/fake_sx.cpp \
    sxfilter/fake_misc.c \
    sxfilter/filter_aes256.c \
    sxfilter/filter_aes256_new.c \
    sxfilter.cpp \
    sxfilter/crypt_blowfish.c \
    xfile.cpp \
    sxlog.cpp \
    volumeconfigwatcher.cpp \
    sxurl.cpp \
    sxerror.cpp \
    clusterconfig.cpp \
    util.cpp

HEADERS += \
    sxcluster.h \
    sxquery.h \
    sxauth.h \
    sxqueryresult.h \
    sxmeta.h \
    sxuserinfo.h \
    sxvolume.h \
    sxfile.h \
    sxfileentry.h \
    sxblock.h \
    sxjob.h \
    sxfilter/fake_misc.h \
    sxfilter/fake_sx.h \
    sxfilter/filter_aes256.h \
    sxfilter.h \
    sxfilter/crypt_blowfish.h \
    sxfilter/sx_input_args.h \
    xfile.h \
    sxlog.h \
    volumeconfigwatcher.h \
    sxurl.h \
    sxerror.h \
    clusterconfig.h \
    util.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

win32 {
    INCLUDEPATH += $$PWD/../3rdparty/openssl-win32/include
    DEPENDPATH += $$PWD/../3rdparty/openssl-win32/include
}
macx {
    INCLUDEPATH += $$PWD/../3rdparty/openssl-osx/include
    DEPENDPATH += $$PWD/../3rdparty/openssl-osx/include
}
