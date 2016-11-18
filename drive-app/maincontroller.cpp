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

#include "maincontroller.h"
#include "settingsdialog.h"
#include "sxcontroller.h"
#include "aboutdialog.h"
#include "certdialog.h"
#include "versioncheck.h"
#include "util.h"
#include "wizard/drivewizard.h"
#include "changepassworddialog.h"
#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QFile>
#include <QTimer>
#include <QDir>
#include "whatsnew.h"
#include <QSettings>
#include <QStandardPaths>
#include <QCursor>
#include <QDesktopWidget>
#include "sharefiledialog.h"
#include <QClipboard>
#include "shellextensions.h"
#include "revisionsdialog.h"
#include <QProcess>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include "trayiconcontroller.h"
#include "volumeconfigwatcher.h"
#include "sxlog.h"
#include "sxfilter.h"
#include "whitelabel.h"
#include "shareconfig.h"

#ifdef Q_OS_WIN
    #include <Windows.h>
#endif

bool MainController::checkCert(QSslCertificate cert, QByteArray fingerprint)
{
    static QByteArray untrustedCert;
    if (untrustedCert == fingerprint)
        return false;
    CertDialog dialog(cert);
    if (dialog.exec() == QMessageBox::Yes)
        return true;
    untrustedCert = fingerprint;
    return false;
}

bool MainController::ask(QString message)
{
    auto answer = QMessageBox::question(nullptr, __applicationName, message, QMessageBox::Yes | QMessageBox::No);
    return answer  == QMessageBox::Yes;
}

MainController::MainController(SxConfig *config, QObject *parent)
    : QObject(parent)
{
    mAvgSpeedUpload = false;
    mUploadSize = 0;
    mDownloadSize = 0;
    mLastInitializationSize = 0;
    connect(VolumeConfigWatcher::instance(), &VolumeConfigWatcher::configChanged, this, &MainController::onVolumeConfigChanged);
    installTranslator(config->desktopConfig().language());
    m_settingsDialog = nullptr;
    m_started = false;
    mUserControlEnabled = true;
    mConfig = config;
    //mFileBrowser = nullptr;
    printConfig();
    mSxController = nullptr;
    mCheckCertCallback = [this](QSslCertificate& cert, bool secondaryCert) -> bool {
        QCryptographicHash sha1(QCryptographicHash::Sha1);
        sha1.addData(cert.toDer());
        QByteArray fprint = sha1.result();
        QByteArray clusterFp = secondaryCert ? mConfig->clusterConfig().secondaryClusterCertFp() : mConfig->clusterConfig().clusterCertFp();

        if (fprint == clusterFp)
            return true;
        bool retVal = false;
        if (QThread::currentThread() == this->thread()) {
            retVal = checkCert(cert, fprint);
        }
        else {
            QMetaObject::invokeMethod(this, "checkCert", Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(bool, retVal),
                                      Q_ARG(QSslCertificate, cert),
                                      Q_ARG(QByteArray, fprint));
        }
        if (retVal) {
            if (secondaryCert)
                mConfig->clusterConfig().setSecondaryClusterCertFp(fprint);
            else
                mConfig->clusterConfig().setClusterCertFp(fprint);
            return true;
        }
        return false;
    };
    mAskGuiCallback = [this](QString message)-> bool {
        bool retVal = false;
        if (QThread::currentThread() == this->thread()) {
            retVal = ask(message);
        }
        else {
            QMetaObject::invokeMethod(this, "ask", Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(bool, retVal),
                                      Q_ARG(QString, message));
        }
        return retVal;
    };
    mSxController = new SxController(config, mCheckCertCallback, mAskGuiCallback, this);
    connect(mSxController, &SxController::sig_fileSynchronised,     this, &MainController::onFileSynchronised);
    connect(mSxController, &SxController::sig_setEtaAction,         this, &MainController::setEtaAction);
    connect(mSxController, &SxController::sig_setEtaCounters,       this, &MainController::setEtaCounters);
    connect(mSxController, &SxController::sig_setProgress,          this, &MainController::setEtaProgress);
    connect(mSxController, &SxController::sig_clusterInitialized,   this, &MainController::onClusterInitialized);
    connect(mSxController, &SxController::sig_fileNotification,     this, &MainController::onFileNotification);
    connect(mSxController, &SxController::sig_lockVolume,           this, &MainController::onLockVolume);
    connect(mSxController, &SxController::sig_gotVcluster,          this, &MainController::onGotVCluster);
    connect(mSxController, &SxController::sig_volumeNameChanged,    this, &MainController::onVolumeNameChanged);
    connect(&m_notificationTimer, &QTimer::timeout,                 this, &MainController::showFilesNotification);
    connect(&SxDatabase::instance(), &SxDatabase::sig_volumeListUpdated, this, &MainController::onVolumeListUpdated);
    lastEtaAction = EtaAction::Inactive;
    setupSystemTray();

    m_wizard = nullptr;
    if (!mConfig->isValid()) {
        QTimer::singleShot(0, this, SLOT(onShowWizard()));
    }

    /*
    connect(ClusterWatcher::instance(), &ClusterWatcher::metaChanged, this, &MainController::clusterMetaChanged);
    if (!__hardcodedSxWeb.isEmpty()) {
        m_sxwebAddress = __hardcodedSxWeb;
        SyncQueue::s_disable_cluster_meta_checking = true;
        m_tray = 0;
        setSharingEnabled(true);
    }

    m_started = false;
    m_config.load();
    m_menu = 0;

    if (m_config.firstRun())
    {
        QTimer::singleShot(0, this, SLOT(onShowWizard()));
    }
    enableLog(m_config.debugLog());

    m_stateController = new StateController(this);
    setupSystemTray();

    m_config.load();
    printConfig();

    m_controller = new SxController(m_stateController, m_config, this);
    m_notificationTimer.setSingleShot(true);

    connect(this, SIGNAL(settingsChanged(const SxConfig&)), m_controller, SLOT(onSettingsChanged(const SxConfig&)));
    connect(m_controller, SIGNAL(fileSynchronised(const QString&, bool)), this, SLOT(onFileSynchronised(const QString&, bool)));
    connect(m_controller, SIGNAL(gotVolumeList(const QList<VolumeInfo>&)), this, SLOT(onVolumeList(const QList<VolumeInfo>&)));
    connect(m_controller, SIGNAL(untrustedCert(const QSslCertificate &)), this, SLOT(onUntrustedCert(const QSslCertificate &)));
    connect(m_controller, SIGNAL(clusterIdRetrieved(const QString &)), this, SLOT(storeClusterUUID(const QString &)));
    connect(m_stateController, SIGNAL(stateChanged(const QString&, const SxState&)), this, SLOT(onStateChanged(const QString&, const SxState&)));
    connect(m_controller, &SxController::gotVCluster, this, &MainController::onGotVCluster);
    connect(this, &MainController::requestRevisionRestore, m_controller, &SxController::requestRevisionRestore);
    */
}

MainController::~MainController()
{
    if (mContextMenu.menu)
        delete mContextMenu.menu;
    /*
    ActiveFilterInput::instance()->disable();
    delete m_controller;
        */
}

static QString EtaActionToString(EtaAction action) {
    switch (action) {
    case EtaAction::Idle:
        return QCoreApplication::translate("MainController", "Up to date");
    case EtaAction::Inactive:
        return QCoreApplication::translate("MainController", "Inactive");
    case EtaAction::Paused:
        return QCoreApplication::translate("MainController", "Paused");
    case EtaAction::ListClusterNodes:
        return QCoreApplication::translate("MainController", "Listing nodes");
    case EtaAction::ListVolumes:
        return QCoreApplication::translate("MainController", "Listing volumes");
    case EtaAction::VolumeInitialScan:
        return QCoreApplication::translate("MainController", "Scanning volume");
    case EtaAction::ListRemoteFiles:
        return QCoreApplication::translate("MainController", "Listing files");
    case EtaAction::UploadFile:
        return QCoreApplication::translate("MainController", "Uploading");
    case EtaAction::DownloadFile:
        return QCoreApplication::translate("MainController", "Downloading");
    case EtaAction::RemoveRemoteFile:
        return QCoreApplication::translate("MainController", "Removing");
    case EtaAction::RemoveLocalFile:
        return QCoreApplication::translate("MainController", "Removing");
    }
    return QString();
}

void MainController::setEtaAction(EtaAction action, qint64 taskCounter, QString file, qint64 size, qint64 speed)
{
    static QList<EtaAction> regularActions = {EtaAction::DownloadFile, EtaAction::UploadFile, EtaAction::RemoveLocalFile, EtaAction::RemoveRemoteFile};
    static QList<EtaAction> specialActions = {EtaAction::ListClusterNodes, EtaAction::ListRemoteFiles, EtaAction::ListVolumes, EtaAction::VolumeInitialScan};

    bool showStatusMenu = regularActions.contains(action) || (taskCounter > 0 && specialActions.contains(action));
    mContextMenu.setStatusMenuEnabled(showStatusMenu);

    if (showStatusMenu) {
        mContextMenu.statusMenu->setTitle(EtaActionToString(action));
        mContextMenu.statusMenu_action->setText(file);
        if (action == EtaAction::UploadFile || action == EtaAction::DownloadFile) {

            bool upload = (action == EtaAction::UploadFile);
            if (upload != mAvgSpeedUpload)
                mAvgSpeedList.clear();
            mAvgSpeedUpload = upload;
            setEtaProgress(size, speed);
        }
        else {
            mContextMenu.statusMenu_progress->setText("");
            mContextMenu.statusMenu_time->setText("");
        }
        lastEtaAction = action;
    }
    else {
        if (action != EtaAction::Paused && action != EtaAction::Inactive) {
            lastEtaAction = EtaAction::Idle;
        }
        else {
            lastEtaAction = action;
            mAvgSpeedList.clear();
        }
        mContextMenu.menu_status->setText(EtaActionToString(lastEtaAction));
    }
}

void MainController::setEtaCounters(uint upload, qint64 uploadSize, uint download, qint64 downloadSize, uint remove)
{
    mUploadSize =uploadSize;
    mDownloadSize = downloadSize;
    if (!mContextMenu.statusMenu)
        return;
    QString text = tr("upload: %1").arg(upload);
    if (upload > 0)
        text += QString(" (%1)").arg(formatSize(uploadSize));
    mContextMenu.statusMenu_uploadCount->setText(text);
    text = tr("download: %1").arg(download);
    if (download > 0)
        text += QString(" (%1)").arg(formatSize(downloadSize));
    mContextMenu.statusMenu_downloadCount->setText(text);
    mContextMenu.statusMenu_removeCount->setText(tr("remove: %1").arg(remove));
}

void MainController::setEtaProgress(qint64 size, qint64 speed)
{
    if (!mContextMenu.statusMenu)
        return;

    if (speed == 0) {
        if (mAvgSpeedUpload) {
            mStartFileInitializeTime = QDateTime::currentDateTime();
            mLastInitializationSize = size;
        }
    }
    else if (mStartFileInitializeTime.isValid()) {
        mInitializationTimeList.append({mLastInitializationSize, mStartFileInitializeTime.msecsTo(QDateTime::currentDateTime())});
        while (mInitializationTimeList.size()>cInitializationTimeListLimit) {
            mInitializationTimeList.removeFirst();
        }
        mStartFileInitializeTime = QDateTime();
    }

    mAvgSpeedList.append(speed);
    while (mAvgSpeedList.count()>cAvgSpeedListLimit) {
        mAvgSpeedList.removeFirst();
    }
    qint64 avgSpeed = 0;
    qint64 fullSize = size;
    if (mAvgSpeedUpload)
        fullSize+=mUploadSize;
    else
        fullSize+=mDownloadSize;
    foreach (auto size, mAvgSpeedList) {
        avgSpeed+=size;
    }
    avgSpeed/=mAvgSpeedList.count();

    QString progressText = QString("size: %1").arg(formatSize(size));
    if (speed > 0)
        progressText += " " + QString("(%2/s)").arg(formatSize(speed));
    mContextMenu.statusMenu_progress->setText(progressText);
    qint64 time = 0;
    if (avgSpeed>0) {
        time = fullSize/avgSpeed;
        qint64 estimatedInitializeTime = 0;
        if ((mUploadSize > 0 || (size>0 && speed==0)) && mAvgSpeedUpload && !mInitializationTimeList.isEmpty()) {
            qint64 tmpSize=0, tmpTime=0;
            foreach (auto pair, mInitializationTimeList) {
                tmpSize+=pair.first;
                tmpTime+=pair.second;
            }
            qint64 fullUploadSize = mUploadSize;
            if (size>0 && speed==0)
                fullUploadSize+=size;
            estimatedInitializeTime = fullUploadSize*tmpTime/tmpSize/1000;
        }
        mContextMenu.statusMenu_time->setText(tr("time: %1").arg(formatEta(time+estimatedInitializeTime)));
    }
    else
        mContextMenu.statusMenu_time->setText(tr("time: %1").arg(tr("calculating")));
}

void MainController::installTranslator(QString lang_code)
{
    QApplication::removeTranslator(&m_translator_qtbase);
    QApplication::removeTranslator(&m_translator_sxdrive);

    if (lang_code != "en")
    {
        m_translator_qtbase.load("qtbase_"+lang_code, ":/translations");
        QApplication::installTranslator(&m_translator_qtbase);
        m_translator_sxdrive.load("sxdrive_"+lang_code, ":/translations");
        QApplication::installTranslator(&m_translator_sxdrive);
    }
    updateTray();
}

void MainController::onClusterInitialized(QString sxweb, QString sxshare)
{
    m_sxwebAddress = sxweb;
    m_sxshareAddress = sxshare;
    if (mContextMenu.menu != nullptr) {
        mContextMenu.menu_showRev->setEnabled(true);
    }
    setSharingEnabled(!sxweb.isEmpty() || !sxshare.isEmpty());
}

void MainController::setSharingEnabled(bool enabled)
{
    if (mContextMenu.menu) {
        mContextMenu.menu_shareFile->setEnabled(true);
        mContextMenu.menu_shareFile->setVisible(!enabled);
        if (mContextMenu.shareRecentMenu && !enabled) {
            mContextMenu.shareRecentMenu->deleteLater();
            mContextMenu.shareRecentMenu = nullptr;
        }
        else if (enabled && !mContextMenu.shareRecentMenu) {
            mContextMenu.shareRecentMenu = new QMenu(tr("Share file"), mContextMenu.menu);
            mContextMenu.menu->insertMenu(mContextMenu.menu_showRev, mContextMenu.shareRecentMenu);
            mContextMenu.shareRecentMenu_showMore = mContextMenu.shareRecentMenu->addAction(tr("Choose file..."), this, SLOT(onShowShareFile()));
            loadRecentHistory();
        }
        mContextMenu.menu_openSxWeb->setVisible(!m_sxwebAddress.isEmpty());
    }
    if (enabled)
        ShellExtensions::instance()->enable(!m_sxshareAddress.isEmpty());
    else
        ShellExtensions::instance()->disable();
}

const SxState &MainController::getState() const
{
    return mSxController->sxState();
}

void MainController::pause()
{
    if (!mUserControlEnabled)
        return;
    on_forcePause();
}

void MainController::resume()
{
    if (!mUserControlEnabled)
        return;
    mSxController->resume();
    if (profile() == "default")
        VersionCheck::instance()->resume();
}

QString MainController::profile() const
{
    return mConfig->profile();
}

bool MainController::isWizardVisible() const
{
    return (m_wizard && m_wizard->isVisible());
}

void MainController::loadRecentHistory()
{
    auto a = SxDatabase::instance().getRecentHistory(false, 5);
    auto b = SxDatabase::instance().getRecentHistory(true, 5);
    foreach (auto file, b) {
        if (!mConfig->volumes().contains(file.first))
            continue;
        QString path = mConfig->volume(file.first).localPath()+file.second;
        onFileSynchronised(path, true);
    }
    foreach (auto file, a) {
        if (!mConfig->volumes().contains(file.first))
            continue;
        QString path = mConfig->volume(file.first).localPath()+file.second;
        onFileSynchronised(path, false);
    }
}

void MainController::setUserControllEnabled(bool enabled)
{
    mUserControlEnabled = enabled;
}

void MainController::on_forcePause()
{
    mSxController->pause();
    if (profile() == "default")
        VersionCheck::instance()->pause();
}

void MainController::updateOpenLocalFolderMenu()
{
    QString vcluster = mConfig->clusterConfig().vcluster();
    if (mContextMenu.openFolderMenu != nullptr) {
        foreach (QAction *action, mContextMenu.openFolderMenu->actions()) {
            mContextMenu.openFolderMenu->removeAction(action);
            action->deleteLater();
        }
        if (mConfig->volumes().isEmpty()) {
            mContextMenu.openFolderMenu->setEnabled(false);
        }
        else {
            mContextMenu.openFolderMenu->setEnabled(true);
            foreach (QString volume, mConfig->volumes()) {
                QString volname = volume;
                if (volname.startsWith(vcluster+"."))
                    volname = volume.mid(vcluster.length()+1);
                QAction *action = mContextMenu.openFolderMenu->addAction(volname, this, SLOT(onOpenLocalFolder()));
                action->setProperty("volume", volume);
            }
        }
    }
}

void MainController::onRestartFilesystemWatcher()
{
    updateOpenLocalFolderMenu();
    updateShellExtensions();
    clearRecentHistory();
    loadRecentHistory();
}

void MainController::updateShellExtensions()
{
    if (!ShellExtensions::instance()->enabled())
        return;
    ShellExtensions::instance()->disable();
    ShellExtensions::instance()->enable(!m_sxshareAddress.isEmpty());
}

void MainController::clearRecentHistory()
{
    auto lastFiles = mContextMenu.lastFilesMenu->actions();
    lastFiles.removeLast();
    foreach (QAction *action, lastFiles) {
        mContextMenu.lastFilesMenu->removeAction(action);
    }
    if (mContextMenu.shareRecentMenu) {
        lastFiles = mContextMenu.shareRecentMenu->actions();
        lastFiles.removeFirst();
        foreach (QAction *action, lastFiles) {
            mContextMenu.shareRecentMenu->removeAction(action);
        }
    }
}

void MainController::onVolumeListUpdated()
{
    QList<SxVolumeEntry> volumeList;
    SxDatabase::instance().getVolumeList(volumeList);
    QStringList volumeNames;
    mVolumesWithFilters.clear();
    foreach (auto vol, volumeList) {
        volumeNames.append(vol.name());
        if (vol.haveFilter())
            mVolumesWithFilters.insert(vol.name());
    }
    foreach (auto encryptedVolume, mEncryptedVolumesTypes.keys()) {
        if (!volumeNames.contains(encryptedVolume))
            mEncryptedVolumesTypes.remove(encryptedVolume);
    }

}

void MainController::onLockVolume(const QString &volume)
{
    mLockedVolumes.insert(volume);
    if (m_settingsDialog != nullptr)
        m_settingsDialog->lockVolume(volume);
}

void MainController::unlockVolume(const QString &volume)
{
    mLockedVolumes.remove(volume);
    mSxController->unlockVolume(volume);
}

void MainController::onVolumeNameChanged()
{
    updateOpenLocalFolderMenu();
    clearRecentHistory();
    loadRecentHistory();
    if (m_settingsDialog != nullptr)
        m_settingsDialog->onVolumeNameChanged();
}

void MainController::onVolumeConfigChanged(const QString &volume, const MetaHash &meta, const MetaHash &customMeta)
{
    VolumeEncryptionType type = GetPasswordDialog::getVolumeEncryptionType(meta, customMeta);
    if (type == VolumeEncryptionType::NOT_ENCRYPTED)
        mEncryptedVolumesTypes.remove(volume);
    else
        mEncryptedVolumesTypes.insert(volume, type);
}

void MainController::setupSystemTray()
{
    m_tray = new QSystemTrayIcon(this);
    mTrayIconController = new TrayIconController(m_tray, &mContextMenu, mSxController, mConfig->desktopConfig().trayIconMark(), this);

    if (!mContextMenu.menu)
    {
        mContextMenu.menu = new QMenu();
        if (profile()!="default") {
            mContextMenu.menu_profile = mContextMenu.menu->addAction("__PROFILE__");
            mContextMenu.menu_profile->setEnabled(false);
            mContextMenu.menu->addSeparator();
        }
        mContextMenu.menu_status = mContextMenu.menu->addAction("__STATUS__");
        mContextMenu.menu_status->setEnabled(false);
        mContextMenu.menu_pauseResume = mContextMenu.menu->addAction("__PAUSE__", this, SLOT(onPauseResume()));
        mContextMenu.menu_pauseResume->setProperty("action_pause", true);
        mContextMenu.menu->addSeparator();
        mContextMenu.openFolderMenu = new QMenu("__OPEN_FOLDER__", mContextMenu.menu);
        mContextMenu.menu->addMenu(mContextMenu.openFolderMenu);
        updateOpenLocalFolderMenu();
        mContextMenu.menu_openSxWeb = mContextMenu.menu->addAction("__OPEN_SXWEB__", this, SLOT(onOpenSXWeb()));
        mContextMenu.menu_openSxWeb->setVisible(!m_sxwebAddress.isEmpty());
        mContextMenu.menu_openFileBrowser = mContextMenu.menu->addAction("__OPEN_FILE_BROWSER__", this, SLOT(onShowFilesBrowser()));
        mContextMenu.menu_openFileBrowser->setVisible(false);
        mContextMenu.menu_shareFile = mContextMenu.menu->addAction("__SHARE_FILE__", this, SLOT(onShowShareFile()));
        if (m_sxwebAddress.isEmpty()) {
            mContextMenu.menu_shareFile->setEnabled(false);
            mContextMenu.shareRecentMenu=0;
        }
        else {
            mContextMenu.menu_shareFile->setVisible(false);
            mContextMenu.shareRecentMenu = mContextMenu.menu->addMenu("__SHARE_FILE__");
            mContextMenu.shareRecentMenu_showMore = mContextMenu.shareRecentMenu->addAction("__CHOOSE_FILE__", this, SLOT(onShowShareFile()));
        }
        mContextMenu.menu_showRev = mContextMenu.menu->addAction("__SHOW_REVISIONS__", this, SLOT(onShowRevisions()));
        mContextMenu.menu_showRev->setEnabled(m_started);
        mContextMenu.volumesMenu = new QMenu(mContextMenu.menu);
        mContextMenu.lastFilesMenu = mContextMenu.menu->addMenu("__RECENTLY_SYNCED__");
        mContextMenu.lastFilesMenu_showMore = mContextMenu.lastFilesMenu->addAction("__SHOW_MORE__", this, SLOT(onShowMoreHistory()));
        mContextMenu.menu->addSeparator();
        mContextMenu.menu_settings = mContextMenu.menu->addAction("__SETTINGS__", this, SLOT(onShowSettings()));
        mContextMenu.menu_about = mContextMenu.menu->addAction("__ABOUT__", this, SLOT(onShowAbout()));
        mContextMenu.menu->addSeparator();
        mContextMenu.menu_quit = mContextMenu.menu->addAction("__QUIT__", this, SLOT(onQuit()));
    }

    m_tray->setContextMenu(mContextMenu.menu);
    m_tray->show();
    updateTray();

    connect(m_tray, &QSystemTrayIcon::messageClicked, [this]()
    {
        if (!m_lastPublicLink.isEmpty())
        {
            QDesktopServices::openUrl(QUrl(m_lastPublicLink, QUrl::TolerantMode));
            m_lastPublicLink.clear();
        }
    });
}
void MainController::updateTray()
{
    if (!mContextMenu.menu)
        return;

    if (mContextMenu.menu_profile)
        mContextMenu.menu_profile->setText(tr("Profile:")+" "+profile());
    if (mContextMenu.menu_pauseResume->property("action_pause").toBool())
        mContextMenu.menu_pauseResume->setText(tr("Pause syncing"));
    else {
        mContextMenu.menu_pauseResume->setText(tr("Resume syncing"));
        lastEtaAction = EtaAction::Paused;
    }
    if (mContextMenu.menu_status)
        mContextMenu.menu_status->setText(EtaActionToString(lastEtaAction));
    else
        mContextMenu.statusMenu->setTitle(EtaActionToString(lastEtaAction));
    mContextMenu.openFolderMenu->setTitle(tr("Open local folder")+"...");
    if (__applicationName == "SXDrive")
        mContextMenu.menu_openSxWeb->setText(tr("Open folder in SXWeb"));
    else
        mContextMenu.menu_openSxWeb->setText(tr("Open folder in web browser"));
    mContextMenu.menu_openFileBrowser->setText(tr("Open remote files browser"));
    mContextMenu.menu_shareFile->setText(tr("Share file..."));
    if (mContextMenu.shareRecentMenu) {
        mContextMenu.shareRecentMenu->setTitle(tr("Share file"));
        mContextMenu.shareRecentMenu_showMore->setText(tr("Choose file..."));
    }
    mContextMenu.menu_showRev->setText(tr("Show revisions..."));
    mContextMenu.lastFilesMenu->setTitle(tr("Recently synced"));
    mContextMenu.lastFilesMenu_showMore->setText(tr("Show more..."));
    mContextMenu.menu_settings->setText(tr("Settings..."));
    mContextMenu.menu_about->setText(tr("About"));
    mContextMenu.menu_quit->setText(tr("&Quit %1").arg(__applicationName));
}

void MainController::onPauseResume()
{
    if (!mUserControlEnabled)
        return;
    bool pause = mContextMenu.menu_pauseResume->property("action_pause").toBool();
    if (pause) {
        on_forcePause();
    }
    else {
        mLockedVolumes.clear();
        resume();
    }
}

void MainController::onOpenLocalFolder()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;
    QString volume = action->property("volume").toString();
    QString localPath = mConfig->volume(volume).localPath();
    const QString uri = "file:///" + localPath+"/";
    logVerbose("Opening: "+ uri);
    QDesktopServices::openUrl(QUrl(uri, QUrl::TolerantMode));
}

void MainController::onOpenSXWeb()
{
    if (m_sxwebAddress.isEmpty())
        return;

    //const QString uri = QString("%1/vol/%2").arg(m_sxwebAddress).arg(m_config.sxVolume());
    const QString uri = m_sxwebAddress;
    logVerbose("opening "+  uri);
    QDesktopServices::openUrl(QUrl(uri, QUrl::TolerantMode));
}

void MainController::onLastFileClicked()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action)
    {
        QString path = action->property("absPath").toString();
        const QFileInfo finfo(path);
        if (finfo.exists())
        {
            const QString uri = "file:///" + finfo.absolutePath();
            logVerbose("Opening: "+ uri);
            QDesktopServices::openUrl(QUrl(uri, QUrl::TolerantMode));
        }
    }
}

void MainController::onFileSynchronised(const QString& path, bool  upload )
{
    Q_UNUSED(upload);
    QString filename = path.split("/").last();
    if (filename == ".sxnewdir" || filename == ".DS_Store")
        return;
    QString volume;
    foreach (QString vol, mConfig->volumes()) {
        QString localPath = mConfig->volume(vol).localPath();
        if (!localPath.endsWith("/"))
            localPath+="/";
        if (path.startsWith(localPath)) {
            volume = vol;
            break;
        }
    }
    QString volName = volume;
    QString vcluster = mConfig->clusterConfig().vcluster();
    if (volName.startsWith(vcluster+"."))
        volName = volume.mid(vcluster.length()+1);

    QAction *action = mContextMenu.lastFilesMenu->addAction(QString("%1: %2").arg(volName).arg(filename), this, SLOT(onLastFileClicked()));
    action->setProperty("absPath", path);
    mContextMenu.lastFilesMenu->removeAction(action);
    mContextMenu.lastFilesMenu->insertAction(mContextMenu.lastFilesMenu->actions().first(), action);

    if (mContextMenu.lastFilesMenu->actions().count() > 6) {
        int count = mContextMenu.lastFilesMenu->actions().count();
        action = mContextMenu.lastFilesMenu->actions().at(count - 2);
        mContextMenu.lastFilesMenu->removeAction(action);
        action->deleteLater();
    }
    if (mContextMenu.shareRecentMenu) {
        if (mVolumesWithFilters.contains(volume))
            return;
        QAction *action = new QAction(QString("%1: %2").arg(volName).arg(filename), mContextMenu.shareRecentMenu);
        action->setProperty("absPath", QString(path).replace("//","/"));
        mContextMenu.shareRecentMenu->insertAction(mContextMenu.shareRecentMenu->actions().first(), action);
        connect(action, &QAction::triggered, [this, action]() {
            QString absPath = action->property("absPath").toString();
            onShowShareFile(absPath);
        });
        if (mContextMenu.shareRecentMenu->actions().count() > 6) {
            int count = mContextMenu.shareRecentMenu->actions().count();
            action = mContextMenu.shareRecentMenu->actions().at(count - 2);
            mContextMenu.shareRecentMenu->removeAction(action);
            action->deleteLater();
        }
    }
}

void MainController::onShowSettings()
{
    onShowSettings(SettingsDialog::SettingsPage::Account);
}

void MainController::onShowWizard()
{
    pause();
    m_wizard = new DriveWizard(mConfig);
    connect(m_wizard, &SxWizard::languageChanged, this, &MainController::installTranslator);
    static QMenu *s_menu = 0;
    static QAction *s_actionAbout = 0;
    static QAction *s_actionWizard = 0;
    static QMetaObject::Connection s_conn;
    if (!mConfig->isValid())
    {
        if (!s_menu)
        {
            s_menu = new QMenu();
            if (profile() != "default")
            {
                s_menu->addAction(tr("Profile:")+" "+profile())->setEnabled(false);
                s_menu->addSeparator();
            }
            s_menu->addAction("Not configured")->setEnabled(false);
            s_actionWizard = s_menu->addAction(tr("Start wizard..."));
            s_actionWizard->setEnabled(false);
            connect(s_actionWizard, &QAction::triggered, [this](bool)
            {
                disconnect(s_conn);
                onShowWizard();
            });
            s_actionAbout = s_menu->addAction(tr("About"), this, SLOT(onShowAbout()));
            s_actionAbout->setEnabled(false);
            s_menu->addSeparator();
            s_menu->addAction(tr("&Quit %1").arg(__applicationName), this, SLOT(onQuit()));
            m_tray->setContextMenu(s_menu);
        }
        else
        {
            s_actionWizard->setEnabled(false);
            s_actionAbout->setEnabled(false);
        }
    }
    if (profile() != "default")
        m_wizard->setWindowTitle(QString("%1 [Profile: "+profile()+"]").arg(__applicationName));
    m_wizard->show();
    mContextMenu.enableAll(false);

    connect(m_wizard, &SxWizard::finished, [this]()
    {
        bool settingsChanged = m_wizard->configChanged();
        m_wizard->deleteLater();
        m_wizard = 0;
        resume();
        //if (m_config.sxCluster().isEmpty())
        if (!mConfig->isValid())
        {
            //m_stateController->error("", "Not configured", SxError::Configuration);
            s_actionAbout->setEnabled(true);
            s_actionWizard->setEnabled(true);
            s_conn = connect(m_tray, &QSystemTrayIcon::activated, [this](QSystemTrayIcon::ActivationReason reason)
            {
                if (reason == QSystemTrayIcon::Trigger)
                {
                    disconnect(s_conn);
                    onShowWizard();
                }
            });
        }
        else
        {
            if (s_menu)
            {
                s_menu->deleteLater();
                s_menu=0;
                s_actionAbout = 0;
                s_actionWizard = 0;
                m_tray->setContextMenu(mContextMenu.menu);
            }
            mContextMenu.enableAll(true);
            if (settingsChanged) {
                updateOpenLocalFolderMenu();
                mSxController->reinitCluster();
            }
        }
        installTranslator(mConfig->desktopConfig().language());
    });
}

void MainController::onShowMoreVolumes()
{
    //onShowSettings(SettingsDialog::SettingsPage::Volumes);
}

void MainController::onShowShareFile(QString file)
{
    if (m_sxwebAddress.isEmpty())
    {
        QMessageBox::about(0, tr("Share file"),
                                 tr("This feature is only available if your sysadmin has enabled it in the cluster. <br><a href='%1'>Learn more...</a> ")
                                 .arg("http://www.sxdrive.io/download/#sxweb"));
        return;
    }
#ifdef Q_OS_WIN
    if (file.contains("~")) {
        file.replace("/", "\\");
        bool isDir = file.endsWith("\\");
        int length = GetLongPathNameW((const wchar_t*)file.utf16(), 0,0);
        wchar_t* buffer = new wchar_t[length];
        length = GetLongPathNameW((const wchar_t*)file.utf16(), buffer, length);
        file = QString::fromUtf16((const ushort*)buffer, length);
        delete [] buffer;
        if (isDir && !file.endsWith("\\"))
            file+="\\";
    }
#endif

    ShareConfig shareConfig(mConfig);
    ShareFileDialog dialog(&shareConfig, m_sxwebAddress, m_sxshareAddress, file);
    if (dialog.exec())
    {
        if (!dialog.errorString().isEmpty())
            QMessageBox::warning(0, tr("Warning"), dialog.errorString());
        else
        {
            QApplication::clipboard()->setText(dialog.publicLink());
            if (m_tray && m_tray->supportsMessages())
            {
                QString msg = tr("A public link to your file has been created and copied to the clipboard.\nClick here to open it in a browser.");
                m_lastPublicLink = dialog.publicLink();
                m_tray->showMessage(tr("Public link"), msg);
            }
            else
            {
                QString msg = tr("A public link to your file has been created and copied to the clipboard.<br>Click <a href='%1'>here</a> to open it in a browser.").arg(dialog.publicLink());
                QMessageBox msgBox;
                msgBox.setWindowTitle(tr("Public link"));
                msgBox.setText(msg);
                msgBox.exec();
            }
        }
    }
}

void MainController::onShowRevisions(QString file)
{
#ifdef Q_OS_WIN
    if (file.contains("~")) {
        file.replace("/", "\\");
        int length = GetLongPathNameW((const wchar_t*)file.utf16(), 0,0);
        wchar_t* buffer = new wchar_t[length];
        length = GetLongPathNameW((const wchar_t*)file.utf16(), buffer, length);
        file = QString::fromUtf16((const ushort*)buffer, length);
        delete [] buffer;
    }
    file = QDir::fromNativeSeparators(file);
#endif
    RevisionsDialog dialog(mConfig, file, mCheckCertCallback, nullptr);
    dialog.exec();
}

void MainController::onShowSettings(SettingsDialog::SettingsPage page)
{
    if (!m_settingsDialog)
    {
        m_settingsDialog = new SettingsDialog(mConfig, mEncryptedVolumesTypes, mLockedVolumes, &mSxController->sxState());
        if (profile() != "default")
        {
            m_settingsDialog->setWindowTitle("Settings [Profile: "+profile()+"]");
            m_settingsDialog->disableAutoUpdate();
        }
        m_settingsDialog->setActivePage(page);
        m_settingsDialog->onStateChanged(mSxController->status());
        m_settingsDialog->show();
        connect(mTrayIconController, &TrayIconController::sig_stateChanged, m_settingsDialog, &SettingsDialog::onStateChanged);
        connect(m_settingsDialog, SIGNAL(destroyed(QObject*)), SLOT(onSettingsDialogDestroyed()));
        connect(m_settingsDialog, SIGNAL(finished(int)), this, SLOT(onSettingsDialogFinished(int)));
        connect(m_settingsDialog, &SettingsDialog::sig_setUserControllEnabled, this, &MainController::setUserControllEnabled);
        connect(m_settingsDialog, &SettingsDialog::sig_forcePause, this, &MainController::on_forcePause);
        connect(m_settingsDialog, &SettingsDialog::sig_restartFilesystemWatcher, mSxController, &SxController::restartFilesystem);
        connect(m_settingsDialog, &SettingsDialog::sig_restartFilesystemWatcher, this, &MainController::onRestartFilesystemWatcher);

        connect(m_settingsDialog, &SettingsDialog::sig_pauseResume, this, &MainController::onPauseResume);
        connect(m_settingsDialog, &SettingsDialog::showWizard, this, &MainController::onShowWizard);
        connect(m_settingsDialog, &SettingsDialog::checkNowForNewVersion, this, &MainController::checkNowForNewVersion);
        connect(m_settingsDialog, &SettingsDialog::sig_volumeUnlocked, this, &MainController::unlockVolume);
        connect(m_settingsDialog, &SettingsDialog::changePasswordClicked, this, &MainController::onChangePasswordClicked);
        connect(m_settingsDialog, &SettingsDialog::sig_updateTrayShape, mTrayIconController, &TrayIconController::updateShape);
        connect(m_settingsDialog, &SettingsDialog::sig_clearWarnings, mSxController, &SxController::clearWarnings);
        connect(m_settingsDialog, &SettingsDialog::sig_requestVolumeList, mSxController, &SxController::sig_requestVolumeList);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainController::onSettingsDialogDestroyed()
{
    m_settingsDialog = nullptr;
}

void MainController::onSettingsDialogFinished(int result)
{
    if (result == QDialog::Accepted) {
        auto desktopConfig = mConfig->desktopConfig();
        QString language = desktopConfig.language();
        m_settingsDialog->storeSettings();
        if (desktopConfig.language() != language)
            installTranslator(desktopConfig.language());
    }
    m_settingsDialog->deleteLater();
}

void MainController::startMainControler()
{
    mSxController->startCluster();
}

void MainController::reinitTray()
{
    /*
    if (m_tray)
    {
        m_tray->deleteLater();
        QMenu *menu = m_tray->contextMenu();
        setupSystemTray();
        m_tray->setContextMenu(menu);
    }
    */
}

void MainController::onShowWhatsNew()
{
    WhatsNew* whatsNew = new WhatsNew();
    whatsNew->showWhatsNew();
}

void MainController::onShowMoreHistory()
{
    onShowSettings(SettingsDialog::SettingsPage::Activity);
}

void MainController::onShowAbout()
{
    AboutDialog about;
    about.show();
    about.raise();
    about.activateWindow();
    about.exec();
}

void MainController::onShowFilesBrowser()
{
    /*
    if (mFileBrowser == nullptr) {
        mFileBrowser = new RemoteFilesBrowser(mConfig);
        connect(mFileBrowser, &RemoteFilesBrowser::destroyed, [this]() {
            mFileBrowser = nullptr;
        });
    }
    mFileBrowser->show();
    */
}

void MainController::onNewVersionAvailable(const QString& ver)
{
    QMessageBox::StandardButton upgrade;
    upgrade = QMessageBox::question(nullptr,
                tr("Update available"),
#if defined Q_OS_WIN || defined Q_OS_MAC
                QString(tr("A new version (<b>%2</b>) of %1 is available.<br>").arg(__applicationName).arg(ver)+tr("Do you want to upgrade now?")),
#else
                QString(tr("A new version (<b>%2</b>) of %1 is available.<br>").arg(__applicationName).arg(ver)+tr("See the upgrade instructions now?")),
#endif
		QMessageBox::Yes|QMessageBox::No,
		QMessageBox::Yes
	    );
    if(upgrade == QMessageBox::Yes) {
#if defined Q_OS_WIN || defined Q_OS_MAC
	mConfig->desktopConfig().setCheckUpdates(mConfig->desktopConfig().checkUpdates(), VersionCheck::instance()->betaCheckingEnabled());
	mConfig->syncConfig();
	QString pwd = QDir::currentPath();
	QDir::setCurrent(QCoreApplication::applicationDirPath());

	auto scriptFailed = [pwd, this]() {
	    disconnect(VersionCheck::instance(), &VersionCheck::signal_updateFailed, 0, 0);
	    QMessageBox msgBox(QMessageBox::Warning, tr("WARNING"), tr("Unable to restart %1. Restart application to run update.").arg(__applicationName));
	    msgBox.exec();
	    QDir::setCurrent(pwd);
	};
	connect(VersionCheck::instance(), &VersionCheck::signal_updateFailed, scriptFailed);
    #ifdef Q_OS_WIN
        if (!QProcess::startDetached("cmd /C start /MIN "+VersionCheck::updateScriptName() + " restart"))
            scriptFailed();
    #else
        if (!QProcess::startDetached(VersionCheck::updateScriptName()+" restart"))
            scriptFailed();
    #endif
#else
	QString upgradeUrl = "http://www.sxdrive.io/download/#sxdrive-desktop";
	QDesktopServices::openUrl(QUrl(upgradeUrl));
#endif
    }
    else
        VersionCheck::instance()->setCheckingBeta(mConfig->desktopConfig().checkBetaVersions());
}

void MainController::onNoNewVersion(bool showDialog)
{
    if (showDialog) {
        QMessageBox::information(m_settingsDialog, tr("%1 Update").arg(__applicationName), tr("No new version available"));
        VersionCheck::instance()->setCheckingBeta(mConfig->desktopConfig().checkBetaVersions());
    }
}

void MainController::onVersionCheckError(bool showDialog, const QString &errorMsg)
{
    if (showDialog) {
        QMessageBox::warning(m_settingsDialog, tr("%1 Update").arg(__applicationName), tr("Checking for new version failed\n")+errorMsg);
        VersionCheck::instance()->setCheckingBeta(mConfig->desktopConfig().checkUpdates());
    }
}

void MainController::onQuit()
{
    QMessageBox message(QMessageBox::Question, __applicationName, tr("Do you want to quit %1?").arg(__applicationName), QMessageBox::Yes | QMessageBox::No);
    message.setWindowFlags(Qt::WindowStaysOnTopHint | message.windowFlags());
    message.show();
    message.raise();
    message.activateWindow();
    int result = message.exec();
    if (result == QMessageBox::Yes)
        QApplication::quit();
}

void MainController::onFileNotification(QString path, QString action)
{
    if (path.split("/").last() == ".sxnewdir")
        return;
    if (mConfig->desktopConfig().notifications()) {
        m_fileNotificationList.insert(path, action);
        if (m_notificationTimer.remainingTime()<500)
            m_notificationTimer.start(500);
    }
}

void MainController::showFilesNotification()
{
    if (m_fileNotificationList.isEmpty())
        return;
    QString msg;
    if (m_fileNotificationList.count() < 5)
    {
        foreach (QString action, m_fileNotificationList.values().toSet()) {
            foreach (QString path, m_fileNotificationList.keys(action)) {
                //QString file = "\""+path.mid(1)+"\"";
                QString file = "\"/"+path+"\"";
                if (!msg.isEmpty())
                    msg+="\n";
                QString trAction;
                if (action=="added")
                    trAction = tr("%1 added").arg(file);
                else if (action=="changed")
                    trAction = tr("%1 changed").arg(file);
                else
                    trAction = tr("%1 removed").arg(file);
                msg+=trAction;
            }
        }
    }
    else
    {
        foreach (QString action, m_fileNotificationList.values().toSet()) {
            if (!msg.isEmpty())
                msg+=", ";
            QString trAction;
            if (action=="added")
                trAction = tr("%n file(s) added", "", m_fileNotificationList.keys(action).count());
            else if (action=="changed")
                trAction = tr("%n file(s) changed", "", m_fileNotificationList.keys(action).count());
            else
                trAction = tr("%n file(s) removed", "", m_fileNotificationList.keys(action).count());
            msg+=trAction;
        }
    }
    m_fileNotificationList.clear();
    m_tray->showMessage(__applicationName, msg);
    m_lastPublicLink.clear();
    m_notificationTimer.start(60*1000);
}

void MainController::onChangePasswordClicked()
{
    QString errorMessage;
    SxCluster *cluster = SxCluster::initializeCluster(mConfig->clusterConfig().sxAuth(), mConfig->clusterConfig().uuid(), mCheckCertCallback, errorMessage);
    if (cluster == nullptr) {
        QMessageBox::warning(m_settingsDialog, __applicationName, errorMessage);
        return;
    }
    ChangePasswordDialog dialog(mConfig->clusterConfig().username(), cluster, false, m_settingsDialog);
    if (dialog.exec()) {
        ClusterConfig &cnf = mConfig->clusterConfig();
        auto cluster = cnf.cluster();
        auto uuid = cnf.uuid();
        auto clusterFp = cnf.clusterCertFp();
        auto address = cnf.address();
        auto ssl = cnf.ssl();
        auto port = cnf.port();
        auto token = dialog.token();
        cnf.setConfig(cluster, uuid, token, address, ssl, port, clusterFp);
        mConfig->syncConfig();
        mSxController->reinitCluster();
    }
    delete cluster;
}

void MainController::printConfig() const
{
    if (!mConfig->isValid())
        return;
    logInfo("Cache directory: " + QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    ClusterConfig &clusterConfig = mConfig->clusterConfig();
    logInfo(QString("Cluster: %1:%2").arg(clusterConfig.cluster()).arg(clusterConfig.port()));
    logInfo(QString("ssl: %1, checkUpdates: %2, notifications: %3, autostart: %4")
             .arg(clusterConfig.ssl())
             .arg(mConfig->desktopConfig().checkUpdates())
             .arg(mConfig->desktopConfig().notifications())
             .arg(mConfig->desktopConfig().autostart()));
    foreach (QString volume, mConfig->volumes()) {
        logInfo(QString("volume: %1, path: %2").arg(volume).arg(mConfig->volume(volume).localPath()));
    }
}

void MainController::clusterMetaChanged()
{
    /*
    if (__hardcodedSxWeb.isEmpty()) {
        auto meta = ClusterWatcher::instance()->meta();
        auto sxweb = meta.value("sxweb_address", QByteArray());
        auto sxshare = meta.value("sxshare_address", QByteArray());
        m_sxwebAddress = QString::fromUtf8(sxweb);
        if (m_sxwebAddress.endsWith("/"))
            m_sxwebAddress.remove(m_sxwebAddress.count()-1,1);
        m_sxshareAddress = QString::fromUtf8(sxshare);
        if (m_sxshareAddress.endsWith("/"))
            m_sxshareAddress.remove(m_sxshareAddress.count()-1,1);
        setSharingEnabled(!m_sxwebAddress.isEmpty() || !m_sxshareAddress.isEmpty());
    }
    if (m_tray)
        m_actionShowRevisions->setEnabled(true);
        */
}

void MainController::onGotVCluster(const QString &vcluster)
{
    if (mConfig->clusterConfig().vcluster() != vcluster) {
        mConfig->clusterConfig().setVcluster(vcluster);
        mConfig->syncConfig();
        if (m_settingsDialog)
        {
            m_settingsDialog->setVCluster(vcluster);
        }
        updateOpenLocalFolderMenu();
    }
}

void ContextMenu::enableAll(bool enable)
{
    openFolderMenu->setEnabled(enable);
    if (shareRecentMenu)
        shareRecentMenu->setEnabled(enable);
    lastFilesMenu->setEnabled(enable);

    menu_pauseResume->setEnabled(enable);
    menu_settings->setEnabled(enable);
    menu_about->setEnabled(enable);
    menu_openFileBrowser->setEnabled(enable);
    menu_openSxWeb->setEnabled(enable);
    if (menu_shareFile)
        menu_shareFile->setEnabled(enable);
}

void ContextMenu::setStatusMenuEnabled(bool enabled)
{
    if (enabled) {
        if (menu_status) {
            menu->removeAction(menu_status);
            menu_status->deleteLater();
            menu_status = nullptr;
        }
        if (!statusMenu) {
            statusMenu = new QMenu(QCoreApplication::translate("MainController","Up to date"), menu);
            statusMenu->setMinimumWidth(300);
            QAction *action = menu->actions().at(menu_profile ? 1 : 0);
            menu->insertMenu(action, statusMenu);
            statusMenu_action = statusMenu->addAction(QCoreApplication::translate("MainController","idle"));
            statusMenu_action->setEnabled(false);
            statusMenu_progress = statusMenu->addAction("...");
            statusMenu_progress->setEnabled(false);
            statusMenu_time = statusMenu->addAction("...");
            statusMenu_time->setEnabled(false);
            QAction* section = statusMenu->addSection(QCoreApplication::translate("MainController","queue:"));
            section->setEnabled(false);
            statusMenu_uploadCount = statusMenu->addAction(QCoreApplication::translate("MainController","upload: %1").arg(0));
            statusMenu_uploadCount->setEnabled(false);
            statusMenu_downloadCount = statusMenu->addAction(QCoreApplication::translate("MainController","download: %1").arg(0));
            statusMenu_downloadCount->setEnabled(false);
            statusMenu_removeCount = statusMenu->addAction(QCoreApplication::translate("MainController","remove: %1").arg(0));
            statusMenu_removeCount->setEnabled(false);
        }
    }
    else {
        if (statusMenu) {
            menu->removeAction(statusMenu->menuAction());
            statusMenu->deleteLater();
            statusMenu = nullptr;
        }
        if (!menu_status) {
            QAction *action = menu->actions().at(menu_profile ? 1 : 0);
            menu_status = new QAction(QCoreApplication::translate("MainController","Up to date"), menu);
            menu_status->setEnabled(false);
            menu->insertAction(action, menu_status);
        }
    }
}
