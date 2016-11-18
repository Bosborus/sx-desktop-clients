#-------------------------------------------------
#
# Project created by QtCreator 2016-09-09T15:07:15
#
#-------------------------------------------------

QT       += core gui network sql concurrent
win32:  QT+= winextras
macx:   QT+= macextras
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
CONFIG += debug_and_release

TARGET = sxscout
TEMPLATE = app

unix {
    target.path = /usr/bin
    INSTALLS += target
}

macx {
    PRE_TARGETDEPS += upgradeScript copyPatch
    upgradeScript.target = upgradeScript
    upgradeScript.depends = FORCE
    upgradeScript.commands = mkdir -p sxscout.app/Contents/MacOS && cp $$_PRO_FILE_PWD_/osxupgrade.sh sxscout.app/Contents/MacOS/sxscout_upgrade.sh
    copyPatch.target = copyPatch
    copyPatch.depends = FORCE
    copyPatch.commands = mkdir -p sxscout.app/Contents/Resources && cp $$_PRO_FILE_PWD_/0001-Add-support-for-dragging-file-promise-on-OSX.patch sxscout.app/Contents/Resources/0001-Add-support-for-dragging-file-promise-on-OSX.patch
    QMAKE_EXTRA_TARGETS += upgradeScript copyPatch

    DISTFILES += 0001-Add-support-for-dragging-file-promise-on-OSX.patch
}

SOURCES += main.cpp \
    detailsdialog.cpp \
    filestableview.cpp \
    mainwindow.cpp \
    progressdialog.cpp \
    tasksdialog.cpp \
    scoutclusterconfig.cpp \
    scoutconfig.cpp \
    scoutshareconfig.cpp \
    settingsdialog.cpp \
    scoutwizard.cpp \
    scoutcontroller.cpp \
    sizevalidator.cpp \
    openingfiledialog.cpp \
    taskdialoglistview.cpp

FORMS += \
    detailsdialog.ui \
    mainwindow.ui \
    progressdialog.ui \
    tasksdialog.ui \
    settingsdialog.ui

HEADERS += \
    detailsdialog.h \
    filestableview.h \
    mainwindow.h \
    progressdialog.h \
    tasksdialog.h \
    scoutclusterconfig.h \
    scoutconfig.h \
    scoutshareconfig.h \
    settingsdialog.h \
    scoutwizard.h \
    version.h \
    scoutcontroller.h \
    sizevalidator.h \
    openingfiledialog.h \
    taskdialoglistview.h

QMAKE_MAC_SDK = macosx10.11
ICON = assets/scout.icns
QMAKE_INFO_PLIST=Info.plist

INCLUDEPATH += $$PWD/../common-gui $$PWD/../scout-core $$PWD/../sx-api
DEPENDPATH += $$PWD/../common-gui $$PWD/../scout-core $$PWD/../sx-api

win32:CONFIG(release, debug|release): {
    LIBS += -L$$OUT_PWD/../common-gui/release/ -lcommon-gui
    LIBS += -L$$OUT_PWD/../scout-core/release/ -lscout-core
    LIBS += -L$$OUT_PWD/../sx-api/release/ -lsx-api
}
else:win32:CONFIG(debug, debug|release): {
    LIBS += -L$$OUT_PWD/../common-gui/debug/ -lcommon-gui
    LIBS += -L$$OUT_PWD/../scout-core/debug/ -lscout-core
    LIBS += -L$$OUT_PWD/../sx-api/debug/ -lsx-api
}
else:unix: {
    LIBS += -L$$OUT_PWD/../common-gui/ -lcommon-gui
    LIBS += -L$$OUT_PWD/../scout-core/ -lscout-core
    LIBS += -L$$OUT_PWD/../sx-api/ -lsx-api
}

win32-g++:CONFIG(release, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/release/libcommon-gui.a
    PRE_TARGETDEPS += $$OUT_PWD/../scout-core/release/libscout-core.a
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/release/libsx-api.a
}
else:win32-g++:CONFIG(debug, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/debug/libcommon-gui.a
    PRE_TARGETDEPS += $$OUT_PWD/../scout-core/debug/libscout-core.a
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/debug/libsx-api.a
}
else:win32:!win32-g++:CONFIG(release, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/release/common-gui.lib
    PRE_TARGETDEPS += $$OUT_PWD/../scout-core/release/scout-core.lib
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/release/sx-api.lib
}
else:win32:!win32-g++:CONFIG(debug, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/debug/common-gui.lib
    PRE_TARGETDEPS += $$OUT_PWD/../scout-core/debug/scout-core.lib
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/debug/sx-api.lib
}
else:unix: {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/libcommon-gui.a
    PRE_TARGETDEPS += $$OUT_PWD/../scout-core/libscout-core.a
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/libsx-api.a
}

macx {
    INCLUDEPATH += $$PWD/../3rdparty/openssl-osx/include
    DEPENDPATH += $$PWD/../3rdparty/openssl-osx/include
    LIBS += \
        -L$$PWD/../3rdparty/openssl-osx/lib/ -lssl -lcrypto \
        "-framework CoreFoundation"  \
        "-framework Cocoa"
    PRE_TARGETDEPS += $$PWD/../3rdparty/openssl-osx/lib/libssl.a
    PRE_TARGETDEPS += $$PWD/../3rdparty/openssl-osx/lib/libcrypto.a
}
else:unix:  LIBS += -lssl -lcrypto
else:win32:LIBS += -L$$PWD/../3rdparty/openssl-win32/lib/ -llibeay32 -lssleay32

RESOURCES += \
    assets/assets.qrc \
    assets/wizard/wizard.qrc

win32 {
    system(echo "IDI_ICON1       ICON    DISCARDABLE     \"assets/scout.ico\"" > app.rc)
    RC_FILE = app.rc
}
