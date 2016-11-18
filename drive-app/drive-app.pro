#-------------------------------------------------
#
# Project created by QtCreator 2014-08-21T08:17:24
#
#-------------------------------------------------

QT       += core gui sql network concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = app

macx {
    PRE_TARGETDEPS += upgradeScript
    upgradeScript.target = upgradeScript
    upgradeScript.depends = FORCE
    upgradeScript.commands = mkdir -p sxdrive.app/Contents/MacOS && cp $$_PRO_FILE_PWD_/osxupgrade.sh sxdrive.app/Contents/MacOS/sxdrive_upgrade.sh
    QMAKE_EXTRA_TARGETS += upgradeScript
}

win32: {
    QT += winextras
    QMAKE_CFLAGS_RELEASE += -Zi
    QMAKE_CXXFLAGS_RELEASE += -Zi
    QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:REF
}

PRE_TARGETDEPS += prepareTranslation
prepareTranslation.target = prepareTranslation
prepareTranslation.depends = FORCE
prepareTranslation.commands = $$[QT_INSTALL_BINS]/lrelease $$_PRO_FILE_PWD_/../sxdrive.pro
QMAKE_EXTRA_TARGETS += prepareTranslation

SOURCES += aboutdialog.cpp \
    main.cpp \
    maincontroller.cpp \
    settingsdialog.cpp \
    synchistoryitemdelegate.cpp \
    whatsnew.cpp \
    sxbutton.cpp \
    shellextensions.cpp \
    revisionsdialog.cpp \
    whitelabel_sxdrive.cpp \
    whitelabel.cpp \
    profilemanager.cpp \
    sxprofilemanagerbutton.cpp \
    trayiconcontroller.cpp \
    synchistorymodel.cpp \
    sxprogressbar.cpp \
    volumeswidget.cpp \
    warningstable.cpp \
    shareconfig.cpp \
    wizard/wizardchoosevolumepage.cpp \
    wizard/drivewizard.cpp

HEADERS  += aboutdialog.h \
    maincontroller.h \
    settingsdialog.h \
    synchistoryitemdelegate.h \
    whatsnew.h \
    sxbutton.h \
    shellextensions.h \
    revisionsdialog.h \
    whitelabel.h \
    profilemanager.h \
    sxprofilemanagerbutton.h \
    sxversion.h \
    trayiconcontroller.h \
    synchistorymodel.h \
    sxprogressbar.h \
    volumeswidget.h \
    warningstable.h \
    shareconfig.h \
    wizard/wizardchoosevolumepage.h \
    wizard/drivewizard.h

FORMS    += aboutdialog.ui \
    settings.ui \
    whatsnew.ui \
    revisionsdialog.ui \
    wizard/wizardchoosevolumepage.ui

include(whitelabel.pri)
DEFINES += "NO_WHITELABEL"

TARGET = sxdrive
DESC_APPNAME="Skylable SXDrive"

isEqual(MAKE_DEB, "yes") {
    DEFINES += "MAKE_DEB"
    QMAKE_POST_LINK += "QT_LIBS='$$QT_INSTALL_LIBS' QT_PLUGINS='$$QT_INSTALL_PLUGINS' $$_PRO_FILE_PWD_/make_deb.sh $$TARGET '$$DESC_APPNAME'"
}

RESOURCES += ../assets/shared.qrc \
    ../assets/tray_shapes.qrc
RESOURCES += ../assets/$$WHITELABEL/graphics.qrc
win32 {
    system(echo "IDI_ICON1       ICON    DISCARDABLE     \"../assets/$$WHITELABEL/sxdrive.ico\"" > app.rc)
    RC_FILE = app.rc
}

RESOURCES += ../translations/sxdrive.qrc
equals(QT_MAJOR_VERSION, 5) {
    lessThan(QT_MINOR_VERSION, 4) : RESOURCES += ../translations/oldtranslations/old-translations.qrc
    equals(QT_MINOR_VERSION, 4) :   RESOURCES += ../translations/qtbasetranslations.qrc
    greaterThan(QT_MINOR_VERSION, 4) : RESOURCES += ../translations/qtbasetranslations.qrc
}

CONFIG += c++11
CONFIG += debug_and_release

QMAKE_MAC_SDK = macosx10.11
ICON = ../assets/$$WHITELABEL/sxdrive.icns
QMAKE_INFO_PLIST=Info.plist

win32: QMAKE_LFLAGS += /SUBSYSTEM:WINDOWS,5.01

unix {
    target.path = /usr/bin
    INSTALLS += target
}

DISTFILES += \
    osxupgrade.sh \
    sxdrive-windows-deploy/deploy.bat \
    sxdrive-windows-deploy/sxdrive_update.bat \
    sxdrive-windows-deploy/Installer.wxs \
    sxdrive-windows-deploy/License.rtf

INCLUDEPATH += $$PWD/../common-gui $$PWD/../drive-core $$PWD/../sx-api
DEPENDPATH += $$PWD/../common-gui $$PWD/../drive-core $$PWD/../sx-api

win32:CONFIG(release, debug|release): {
    LIBS += -L$$OUT_PWD/../common-gui/release/ -lcommon-gui
    LIBS += -L$$OUT_PWD/../drive-core/release/ -ldrive-core
    LIBS += -L$$OUT_PWD/../sx-api/release/ -lsx-api
}
else:win32:CONFIG(debug, debug|release): {
    LIBS += -L$$OUT_PWD/../common-gui/debug/ -lcommon-gui
    LIBS += -L$$OUT_PWD/../drive-core/debug/ -ldrive-core
    LIBS += -L$$OUT_PWD/../sx-api/debug/ -lsx-api
}
else:unix: {
    LIBS += -L$$OUT_PWD/../common-gui/ -lcommon-gui
    LIBS += -L$$OUT_PWD/../drive-core/ -ldrive-core
    LIBS += -L$$OUT_PWD/../sx-api/ -lsx-api
}

win32-g++:CONFIG(release, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/release/libcommon-gui.a
    PRE_TARGETDEPS += $$OUT_PWD/../drive-core/release/libdrive-core.a
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/release/libsx-api.a
}
else:win32-g++:CONFIG(debug, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/debug/libcommon-gui.a
    PRE_TARGETDEPS += $$OUT_PWD/../drive-core/debug/libdrive-core.a
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/debug/libsx-api.a
}
else:win32:!win32-g++:CONFIG(release, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/release/common-gui.lib
    PRE_TARGETDEPS += $$OUT_PWD/../drive-core/release/drive-core.lib
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/release/sx-api.lib
}
else:win32:!win32-g++:CONFIG(debug, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/debug/common-gui.lib
    PRE_TARGETDEPS += $$OUT_PWD/../drive-core/debug/drive-core.lib
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/debug/sx-api.lib
}
else:unix: {
    PRE_TARGETDEPS += $$OUT_PWD/../common-gui/libcommon-gui.a
    PRE_TARGETDEPS += $$OUT_PWD/../drive-core/libdrive-core.a
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/libsx-api.a
}

macx {
    INCLUDEPATH += $$PWD/../3rdparty/openssl-osx/include
    DEPENDPATH += $$PWD/../3rdparty/openssl-osx/include
    LIBS += -L$$PWD/../3rdparty/openssl-osx/lib/ -lssl -lcrypto
    PRE_TARGETDEPS += $$PWD/../3rdparty/openssl-osx/lib/libssl.a
    PRE_TARGETDEPS += $$PWD/../3rdparty/openssl-osx/lib/libcrypto.a
}
else:unix:  LIBS += -lssl -lcrypto
else:win32:LIBS += -L$$PWD/../3rdparty/openssl-win32/lib/ -llibeay32 -lssleay32

