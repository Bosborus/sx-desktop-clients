/*
 *  Copyright (C) 2012-2016 Skylable Ltd. <info-copyright@skylable.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Special exception for linking this software with OpenSSL:
 *
 *  In addition, as a special exception, Skylable Ltd. gives permission to
 *  link the code of this program with the OpenSSL library and distribute
 *  linked combinations including the two. You must obey the GNU General
 *  Public License in all respects for all of the code used other than
 *  OpenSSL. You may extend this exception to your version of the program,
 *  but you are not obligated to do so. If you do not wish to do so, delete
 *  this exception statement from your version.
 */

#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include <QSystemTrayIcon>
#include <QSslCertificate>
#include <sxconfig.h>
#include <QList>
#include "settingsdialog.h"
#include "sxqueue.h"
#include <QTimer>
#include <QTranslator>
#include <functional>

class SxController;
class SettingsDialog;
class VersionCheck;
class SxState;
class QMenu;
class QAction;
class SxWizard;
class TrayIconController;

struct ContextMenu {
    QMenu *menu;
    QMenu *lastFilesMenu;
    QMenu *volumesMenu;
    QMenu *shareRecentMenu;
    QMenu *openFolderMenu;
    QMenu *statusMenu;

    QAction *menu_profile;
    QAction *menu_status;
    QAction *menu_pauseResume;
    QAction *menu_openSxWeb;
    QAction *menu_openFileBrowser;
    QAction *menu_shareFile;
    QAction *menu_showRev;
    QAction *menu_settings;
    QAction *menu_about;
    QAction *menu_quit;
    QAction *lastFilesMenu_showMore;
    QAction *shareRecentMenu_showMore;

    QAction *statusMenu_action;
    QAction *statusMenu_progress;
    QAction *statusMenu_time;
    QAction *statusMenu_uploadCount;
    QAction *statusMenu_downloadCount;
    QAction *statusMenu_removeCount;

    //QAction *m_actionOpenFolder;
    //QAction *m_menuEtaSize;
    //QAction *m_menuEtaSpeed;


    ContextMenu() {
        menu = nullptr;
        lastFilesMenu = nullptr;
        volumesMenu = nullptr;
        shareRecentMenu = nullptr;
        menu_pauseResume = nullptr;
        lastFilesMenu_showMore = nullptr;
        shareRecentMenu_showMore = nullptr;
        menu_status = nullptr;
        menu_quit = nullptr;
        menu_about = nullptr;
        menu_settings = nullptr;
        menu_profile = nullptr;
        openFolderMenu = nullptr;
        menu_openFileBrowser = nullptr;
        menu_openSxWeb = nullptr;
        menu_shareFile = nullptr;
        menu_showRev = nullptr;

        statusMenu = nullptr;
        statusMenu_action = nullptr;
        statusMenu_progress = nullptr;
        statusMenu_time = nullptr;
        statusMenu_uploadCount = nullptr;
        statusMenu_downloadCount = nullptr;
        statusMenu_removeCount = nullptr;
    }
    void enableAll(bool enable);
    void setStatusMenuEnabled(bool enabled);
};

class MainController: public QObject
{
    Q_OBJECT
public:
    MainController(SxConfig* config, QObject *parent = nullptr);
    ~MainController();
signals:
private slots:
    void setEtaAction(EtaAction action, qint64 taskCounter,QString file, qint64 size, qint64 speed);
    void setEtaCounters(uint upload, qint64 uploadSize, uint download, qint64 downloadSize, uint remove);
    void setEtaProgress(qint64 size, qint64 speed);

private:
    void setupSystemTray();
    void updateOpenLocalFolderMenu();
    void updateShellExtensions();
    void clearRecentHistory();
private:
    const int cAvgSpeedListLimit = 100;
    const int cInitializationTimeListLimit = 30;
    bool m_started;
    SxController *mSxController;
    SxConfig* mConfig;
    ContextMenu mContextMenu;
    QString m_sxwebAddress;
    QString m_sxshareAddress;
    QString m_lastPublicLink;
    QSystemTrayIcon *m_tray;
    TrayIconController *mTrayIconController;
    SettingsDialog *m_settingsDialog;
    QTranslator m_translator_qtbase;
    QTranslator m_translator_sxdrive;
    bool mUserControlEnabled;
    QHash<QString, QString> m_fileNotificationList;
    QTimer m_notificationTimer;
    QHash<QString, VolumeEncryptionType> mEncryptedVolumesTypes;
    QSet<QString> mLockedVolumes;
    QSet<QString> mVolumesWithFilters;
    SxWizard *m_wizard;
    EtaAction lastEtaAction;
    qint64 mUploadSize;
    qint64 mDownloadSize;
    QList<qint64> mAvgSpeedList;
    QDateTime mStartFileInitializeTime;
    qint64 mLastInitializationSize;
    bool mAvgSpeedUpload;
    QList<QPair<qint64, qint64>> mInitializationTimeList;
// ---------------------------------------------
public:
    void updateTray();
    void setSharingEnabled(bool enabled);
    const SxState &getState() const;
    void pause();
    void resume();
    QString profile() const;
    bool isWizardVisible() const;
    void loadRecentHistory();
    std::function<bool(QSslCertificate&,bool)> mCheckCertCallback;
    std::function<bool(QString)> mAskGuiCallback;

public Q_SLOTS:
    void onOpenLocalFolder();
    void onOpenSXWeb();
    void onShowSettings(SettingsDialog::SettingsPage page);
    void onShowSettings();
    void onShowWizard();
    void onShowAbout();
    void onShowFilesBrowser();
    void onQuit();
    void onNewVersionAvailable(const QString& ver);
    void onNoNewVersion(bool showDialog);
    void onVersionCheckError(bool showDialog, const QString &errorMsg);
    void onShowMoreHistory();
    void onShowMoreVolumes();
    void onShowShareFile(QString file = "");
    void onShowRevisions(QString file = "");
    void startMainControler();
    void reinitTray();
    void onShowWhatsNew();
    void installTranslator(QString lang_code);
    void onClusterInitialized(QString sxweb, QString sxshare);

protected Q_SLOTS:
    void onFileSynchronised(const QString& path, bool upload);
    void onPauseResume();
    void onLastFileClicked();
    void onSettingsDialogFinished(int result);
    void onSettingsDialogDestroyed();
    void onFileNotification(QString path, QString action);
    void showFilesNotification();
    void onChangePasswordClicked();
    void printConfig() const;
    void clusterMetaChanged();
    void onGotVCluster(const QString &vcluster);
    void setUserControllEnabled(bool enabled);
    void on_forcePause();
    void onRestartFilesystemWatcher();
    void onVolumeConfigChanged(const QString &volume, const QHash<QString, QByteArray> &meta, const QHash<QString, QByteArray> &customMeta);
    void onVolumeListUpdated();
    void onLockVolume(const QString &volume);
    void unlockVolume(const QString &volume);
    void onVolumeNameChanged();
public Q_SLOTS:
    bool checkCert(QSslCertificate cert, QByteArray fingerprint);
    bool ask(QString message);

Q_SIGNALS:
    void settingsChanged(const SxConfig& config);
    void checkNowForNewVersion(bool checkBeta);
    void startingSxController();
    void requestRevisionRestore(const QString& file, const QString& revision, const QString& target);

    /*
    bool m_trayOnBottom;
    SxController *m_controller;
    StateController *m_stateController;
    SxConfig m_config;
    QList<VolumeInfo> m_volumes;

    QList<QString> m_lastDownloads;
    */
};

#endif
