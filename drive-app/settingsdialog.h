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

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include "ui_settings.h"
#include "dropframe.h"
#include "profilemanager.h"
#include "sxprofilemanagerbutton.h"
#include "sxdatabase.h"
#include "synchistorymodel.h"
#include "sxprogressbar.h"
#include "volumeswidget.h"
#include "sxauth.h"
#include "sxstate.h"

#include <QLocalServer>
#include <QMap>
#include <QPair>
#include <QTimer>

class QStringListModel;
class QProgressBar;
class SxConfig;
class SyncHistory;

class SettingsDialog : public QDialog, private Ui::Settings
{
    Q_OBJECT
public:
    enum SettingsPage
    {
        Account = 0,
        General,
        Activity,
        Volumes,
        SelectiveSync,
        ProfileManager,
        Warnings,
        Advanced
    };

    SettingsDialog(SxConfig* config, QHash<QString, VolumeEncryptionType> &encryptedVolumesTypes, QSet<QString> &lockedVolumes, const SxState* sxState);
    ~SettingsDialog();
private:
    void setupAccountPage();
    void setupGeneralPage();
    void setupActivityPage();
    void setupVolumesPage();
    void setupSelectiveSyncPage();
    void setupProfileManagerPage();
    void setupWarningsPage();
    void setupLoggingPage();

public:
    void storeSettings();
    void disableAutoUpdate();
    void setVCluster(const QString &vcluster);
    void lockVolume(const QString &volume);
    void onVolumeNameChanged();

private slots:
    void updateVolumeList();
    void onOpenLog();

Q_SIGNALS:
    void sig_forcePause();
    void sig_volumeUnlocked(const QString &volume);

    void clearLog();
    void showWizard();
    void checkNowForNewVersion(bool checkBeta);
    void changePasswordClicked();
    void sig_setUserControllEnabled(bool enabled);
    void sig_paused();
    void sig_restartFilesystemWatcher();
    void sig_pauseResume();
    void sig_updateTrayShape(QPair<QString, QString> &);
    void sig_clearWarnings();
    void sig_requestVolumeList();

public Q_SLOTS:
    void setActivePage(SettingsPage page);
    void onStateChanged(SxStatus status);
    //void onVolumeList(const QList<VolumeInfo>& volumes);
    //void onStateChanged(const QString&, const SxState& state);

protected Q_SLOTS:
    void onAddIgnoredPath();
    void onRemoveIgnoredpath();
    void onPathSelected(const QModelIndex& index);
    void onConfigPageChanged();
    void onConfigPageChanged(int row);
    void beforeLogInsert();
    void onLogsInserted(const QModelIndex &, int first, int last);
    void on_buttonWizard_clicked();
    void on_buttonCheckNow_clicked();
    void initializeProfilesManager();

private:
    struct ProfilePtr
    {
        QLabel *name;
        QLabel *status;
        SxProfileManagerButton *settings;
        SxProfileManagerButton *startStop;
        SxProfileManagerButton *pauseResume;
        SxProfileManagerButton *removeProfile;

        static ProfilePtr createEmptyWidgets();
    };

    struct SelectiveSyncStruct {
        bool whitelist;
        QStringListModel *model;
        QList<QRegExp> regexpList;
    };

    // ---------------new variables--------------
    static QMap<QString, QRegExp::PatternSyntax> s_patterns;
    SxConfig *mConfig;
    SyncHistoryModel* mHistory;
    QHash<QString, VolumeEncryptionType> &mEncryptedVolumesTypes;
    QSet<QString> &mLockedVolumes;
    QList<ProfilePtr> m_profilesWidgets;
    QHash<QString, SelectiveSyncStruct*> m_selectiveSync;
    VolumesWidget *scrollArea;
    const SxState *mSxState;

    //----------------old variables--------------
    int m_currentPageIndex;
    QString m_vcluster;
    bool m_profilesInitialized;
    QTimer mProfileUpdateTimer;
    QString m_profile;
    QString m_rootDir;
    SxAuth mAuth;

    void updateProfileWidgets(SettingsDialog::ProfilePtr &widgets, const ProfileManager::ProfileStatus &status);
protected:
    void paintEvent(QPaintEvent *e);
    bool m_scrolledToBottom;
private slots:
    void on_pushButton_clicked();
    void on_changePasswordButton_clicked();
    void on_checkNewVersion_stateChanged(int arg1);
    void on_enableDebugLog_stateChanged(int arg1);

    void addRegexp(QString pattern=QString(), QString syntax=QString());
    void removeRegexp();
    void onUpdateProfiles(const QList<ProfileManager::ProfileStatus> &list);

    void onProfileSettingsClicked();
    void onProfileStartStopClicked();
    void onProfilePauseResumeClicked();
    void onProfileRemoveClicked();
    void on_pushButton_2_clicked();
    void on_sendLogsButton_clicked();
    void on_historyListView_doubleClicked(const QModelIndex &index);
    void on_clearWarningsButton_clicked();
};

#endif
