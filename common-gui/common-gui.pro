#-------------------------------------------------
#
# Project created by QtCreator 2016-09-12T10:27:19
#
#-------------------------------------------------

TARGET = common-gui
TEMPLATE = lib
CONFIG += staticlib c++11

QT += gui widgets network concurrent

unix {
    target.path = /usr/lib
    INSTALLS += target
}

FORMS += \
    sharefiledialog.ui \
    getpassworddialog.ui \
    wizard/wizardconnecttopage.ui \
    wizard/wizardsetupcompletepage.ui \
    wizard/wizardstartpage.ui \
    downloaddialog.ui

SOURCES += singleapp/qtlocalpeer.cpp singleapp/qtlockedfile.cpp singleapp/qtlockedfile_unix.cpp singleapp/qtlockedfile_win.cpp singleapp/qtsingleapplication.cpp
HEADERS += singleapp/qtlocalpeer.h singleapp/qtlockedfile.h singleapp/qtsingleapplication.h

HEADERS += \
    sharefiledialog.h \
    abstractshareconfig.h \
    certdialog.h \
    getpassworddialog.h \
    changepassworddialog.h \
    coloredframe.h \
    wizard/sxwizard.h \
    wizard/sxwizardpage.h \
    wizard/wizardconnecttopage.h \
    wizard/wizardsetupcompletepage.h \
    wizard/wizardstartpage.h \
    dropframe.h \
    translations.h \
    versioncheck.h \
    downloaddialog.h \
    logtableview.h \
    logsmodel.h

SOURCES += \
    sharefiledialog.cpp \
    certdialog.cpp \
    abstractshareconfig.cpp \
    getpassworddialog.cpp \
    changepassworddialog.cpp \
    coloredframe.cpp \
    wizard/sxwizard.cpp \
    wizard/sxwizardpage.cpp \
    wizard/wizardconnecttopage.cpp \
    wizard/wizardsetupcompletepage.cpp \
    wizard/wizardstartpage.cpp \
    dropframe.cpp \
    translations.cpp \
    versioncheck.cpp \
    downloaddialog.cpp \
    logsmodel.cpp \
    logtableview.cpp

INCLUDEPATH += $$PWD/../sx-api
DEPENDPATH += $$PWD/../sx-api


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
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/release/libsx-api.a
}
else:win32-g++:CONFIG(debug, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/debug/libsx-api.a
}
else:win32:!win32-g++:CONFIG(release, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/release/sx-api.lib
}
else:win32:!win32-g++:CONFIG(debug, debug|release): {
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/debug/sx-api.lib
}
else:unix: {
    PRE_TARGETDEPS += $$OUT_PWD/../sx-api/libsx-api.a
}
