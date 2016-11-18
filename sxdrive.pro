QMAKE_MAC_SDK = macosx10.11
TEMPLATE = subdirs
CONFIG += c++11 debug_and_release
SUBDIRS += sx-api drive-core common-gui drive-app \
           scout-core scout-app
CONFIG += ordered

win32: {
    QMAKE_CFLAGS_RELEASE += -Zi
    QMAKE_CXXFLAGS_RELEASE += -Zi
    QMAKE_LFLAGS_RELEASE += /DEBUG /OPT:REF
}

DISTFILES += \
    sxdrive.supp \
    console.supp

TRANSLATIONS += translations/sxdrive_pl.ts \
                translations/sxdrive_it.ts \
                translations/sxdrive_hu.ts \
                translations/sxdrive_ro.ts \
                translations/sxdrive_de.ts \
                translations/sxdrive_fr.ts \
                translations/sxdrive_template.ts

# qt translations
# http://l10n-files.qt.io/l10n-files/

WHITELABEL="sxdrive"
WHITELABEL_FILE=$$_PRO_FILE_PWD_/drive-app/whitelabel_$$WHITELABEL'.'cpp
!exists( $$WHITELABEL_FILE ) {
    error(FILE $$WHITELABEL_FILE is missing)
}

OUTPUT_LINE=WHITELABEL=\'\'$$WHITELABEL
write_file($$_PRO_FILE_PWD_/drive-app/whitelabel.pri, OUTPUT_LINE)
unix:!macx: {
    MAKE_DEB=$$(SXDRIVE_MAKEDEB)
    isEmpty(MAKE_DEB): MAKE_DEB="no"
    OUTPUT_LINE=MAKE_DEB=\'\'$$MAKE_DEB
    write_file($$_PRO_FILE_PWD_/drive-app/whitelabel.pri, OUTPUT_LINE, append)
}
OUTPUT_LINE=QT_INSTALL_LIBS=\'\'$$system($$QMAKE_QMAKE -query QT_INSTALL_LIBS)
write_file($$_PRO_FILE_PWD_/drive-app/whitelabel.pri, OUTPUT_LINE, append)
OUTPUT_LINE=QT_INSTALL_PLUGINS=\'\'$$system($$QMAKE_QMAKE -query QT_INSTALL_PLUGINS)
write_file($$_PRO_FILE_PWD_/drive-app/whitelabel.pri, OUTPUT_LINE, append)

macx: system(sh $$_PRO_FILE_PWD_/generate_plist.sh $$WHITELABEL_FILE $$_PRO_FILE_PWD_/)
