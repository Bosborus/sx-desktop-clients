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

#include <QFileDialog>
#include <QStringListModel>
#include <QMessageBox>
#include <QProgressBar>
#include <QSet>
#include <QHBoxLayout>
#include <QDebug>
#include <QDesktopWidget>
#include <QInputDialog>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSpacerItem>
#include <QScrollBar>
#include <QPair>
#include <QDesktopServices>
#include <QPainter>
#include <QComboBox>
#include <QBitmap>
#include <QColorDialog>
#include "sxconfig.h"
#include "synchistoryitemdelegate.h"
#include "util.h"
#include "sxversion.h"
#include "versioncheck.h"
#include "wizard/sxwizard.h"
#include "sxprofilemanagerbutton.h"
#include "translations.h"
#include "logsmodel.h"
#include "settingsdialog.h"
#include "getpassworddialog.h"
#include "sxauth.h"
#include "warningstable.h"

#define CLASS_NAME "SettingsDialog:"

#include "profilemanager.h"
#include "whitelabel.h"

void SettingsDialog::updateVolumeList()
{
    QList<SxVolumeEntry> volumes;
    if (!SxDatabase::instance().getVolumeList(volumes)) {
        scrollArea->showMessage(tr("unable to list volumes"));
        return;
    }
    QHash<QString, QString> config;
    foreach (QString volume, mConfig->volumes()) {
        config.insert(volume, mConfig->volume(volume).localPath());
    }

    scrollArea->updateVolumes(m_vcluster, volumes, config, mLockedVolumes, mEncryptedVolumesTypes);
}

SettingsDialog::~SettingsDialog()
{
    ignoredPathList->setModel(nullptr);
    foreach (auto ss, m_selectiveSync) {
        delete ss->model;
        delete ss;
    }
}

void SettingsDialog::setActivePage(SettingsPage page)
{
    configPagesList->setCurrentRow(static_cast<int>(page));
}

SettingsDialog::SettingsDialog(SxConfig *config, QHash<QString, VolumeEncryptionType> &encryptedVolumesTypes, QSet<QString> &lockedVolumes, const SxState* sxState)
    : QDialog(nullptr, Qt::WindowTitleHint | Qt::WindowCloseButtonHint
              #ifdef Q_OS_MAC
              | Qt::WindowStaysOnTopHint
              #endif
              ),
      mEncryptedVolumesTypes(encryptedVolumesTypes),
      mLockedVolumes(lockedVolumes)
{
    setupUi(this);
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);
    mConfig = config;
    m_vcluster = mConfig->clusterConfig().vcluster();

    if (mConfig->isValid())
        mAuth = mConfig->clusterConfig().sxAuth();
    mSxState = sxState;

    setupAccountPage();
    setupGeneralPage();
    setupActivityPage();
    setupVolumesPage();
    setupSelectiveSyncPage();
    setupProfileManagerPage();
    setupWarningsPage();
    setupLoggingPage();

    m_currentPageIndex = 0;
    configPagesList->setCurrentRow(m_currentPageIndex);
    configPagesStack->setCurrentIndex(m_currentPageIndex);
    connect(configPagesList, SIGNAL(itemSelectionChanged()), this, SLOT(onConfigPageChanged()));
    connect(configPagesList, SIGNAL(currentRowChanged(int)), this, SLOT(onConfigPageChanged(int)));
    connect(configPagesList, SIGNAL(currentRowChanged(int)), configPagesStack, SLOT(setCurrentIndex(int)));

    if (isRetina()) {
        logo->setPixmap(QPixmap(":/icons/systemicon@2x.png"));
        label_wizard->setPixmap(QPixmap(":/icons/sxwizard@2x.png"));
    }
    versionLabel->setText(SXVERSION);
    versionLabel->setStyleSheet(QString("color: %1;").arg(__color_versionText));
    versionLabel0->setStyleSheet(QString("color: %1;").arg(__color_versionText));
}

void SettingsDialog::setupAccountPage()
{
    ClusterConfig& config = mConfig->clusterConfig();
    sxCluster->setText(config.cluster());
    sslEnable->setChecked(config.ssl());
    sslPort->setValue(config.port());
    sxAddress->setText(config.address());
    usernameLineEdit->setText(config.username());
}

void SettingsDialog::setupGeneralPage()
{
    DesktopConfig &config = mConfig->desktopConfig();
    notifications->setChecked(config.notifications());
    checkNewVersion->setChecked(config.checkUpdates());
    if (!config.checkUpdates())
    {
        checkBetaVersion->setEnabled(false);
        checkBetaVersion->setChecked(false);
    }
    else
        checkBetaVersion->setChecked(config.checkBetaVersions());
    autoStart->setChecked(config.autostart());

#if defined Q_OS_WIN || defined Q_OS_MAC
    checkNewVersion->setText(tr("Automatically install updates"));
#else
    checkBetaVersion->setVisible(false);
    int row, column, unused, index;
    QGridLayout *pageGeneralGridLayout = qobject_cast<QGridLayout*>(frame_2->layout());
    if (pageGeneralGridLayout)
    {
        index = pageGeneralGridLayout->indexOf(buttonCheckNow);
        pageGeneralGridLayout->getItemPosition(index, &row, &column, &unused, &unused);
        pageGeneralGridLayout->takeAt(index);
        pageGeneralGridLayout->addWidget(buttonCheckNow, row, column);
    }
#endif
    languageSelector->addItem("English");
    languageSelector->addItems(Translations::instance()->availableLanguages());
    QString langCode = config.language();
    QString langNative = Translations::instance()->nativeLanguage(langCode);
    languageSelector->setCurrentText(langNative);

    connect(checkBetaVersion, &QCheckBox::stateChanged, [this](int arg1) {
        if (arg1)
            QMessageBox::warning(this, tr("Warning"), tr("Beta releases are not thoroughly tested - use at your own risk."));
    });

#ifdef NO_WHITELABEL
    QDir dir(":/shapes/");
    foreach (QString file, dir.entryList()) {
        if (file.endsWith(".png"))
            comboBoxShape->addItem(file.mid(0, file.length()-4));
    }

    auto updateColorButton = [this]() {
        QString style = QString("background-color: %1").arg(buttonColor->text());
        buttonColor->setStyleSheet(style);
    };
    auto drawTrayIconPreview = [this, updateColorButton]() {
        QPixmap pixmap(64, 64);
        QRectF target(0, 0, pixmap.width(), pixmap.height());
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        QString bgName;
        QString fgName;

        if (comboBoxShape->currentIndex() == 0) {
            bgName = ":/shapes/alt/background.png";
            fgName = ":/shapes/alt/tray@2x.png";
        }
        else {
            bgName = ":/shapes/"+comboBoxShape->currentText();
            fgName = ":/shapes/tray/tray@2x.png";
        }
        QPixmap shape(bgName);
        QPixmap foreground(fgName);
        QPixmap coloredShape( shape.size() );
        QColor color(buttonColor->text());
        coloredShape.fill( color );
        coloredShape.setMask( shape.createMaskFromColor( Qt::transparent ) );
        QRectF source(0, 0, coloredShape.width(), coloredShape.height());
        painter.drawPixmap(target, coloredShape, source);
        painter.drawPixmap(target, foreground, target);
        trayIconPreviewButton->setIcon(QIcon(pixmap));
    };

    auto pickColor = [this, updateColorButton, drawTrayIconPreview]() {
        QColorDialog dialog;
        dialog.setWindowFlags(dialog.windowFlags() | Qt::WindowStaysOnTopHint);
        dialog.setCurrentColor(QColor(buttonColor->text()));
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
        if (dialog.exec()) {
            buttonColor->setText(dialog.currentColor().name());
            updateColorButton();
            drawTrayIconPreview();
        }
    };

    auto savedShape = mConfig->desktopConfig().trayIconMark();
    if (!savedShape.first.isEmpty()) {
        int index = comboBoxShape->findText(savedShape.first);
        if (index > 0) {
            comboBoxShape->setCurrentIndex(index);
        }
    }
    if (savedShape.second.isEmpty())
        buttonColor->setText(__color_background);
    else
        buttonColor->setText(savedShape.second);

    updateColorButton();
    drawTrayIconPreview();
    connect(comboBoxShape, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), drawTrayIconPreview);
    connect(buttonColor, &QPushButton::clicked, pickColor);

#else
    trayIconPreviewButton->setVisible(false);
    comboBoxShape->setVisible(false);
    buttonColor->setVisible(false);
#endif
}

void SettingsDialog::setupActivityPage()
{
    QAbstractItemDelegate *renderRowDelegate = new SyncHistoryItemDelegate(m_vcluster, this);
    mHistory = new SyncHistoryModel();
    historyListView->setModel(mHistory);
    historyListView->setItemDelegate(renderRowDelegate);
    connect(pauseButton, &QPushButton::clicked, this, &SettingsDialog::sig_pauseResume);
}

void SettingsDialog::setupVolumesPage()
{
    scrollArea = new VolumesWidget(this);
    scrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    pageVolumes->layout()->addWidget(scrollArea);
    if (mConfig->isValid()) {
        scrollArea->setAuth(mAuth);
        scrollArea->setClusterUuid(mConfig->clusterConfig().uuid());
    }

    updateVolumeList();
    connect(&SxDatabase::instance(), &SxDatabase::sig_volumeListUpdated, this, &SettingsDialog::updateVolumeList);
    connect(scrollArea, &VolumesWidget::sig_volumeUnlocked, this, &SettingsDialog::sig_volumeUnlocked);
}

void SettingsDialog::setupSelectiveSyncPage()
{
    static QString currentVolume;
    auto loadVolume = [this](const QString &volume) {
        if (!currentVolume.isEmpty() && m_selectiveSync.contains(currentVolume)) {
            auto ss = m_selectiveSync.value(currentVolume);
            ss->regexpList.clear();
            for (int i = 0; i<regexpTable->rowCount(); i++) {
                QTableWidgetItem *item = regexpTable->item(i, 0);
                if (!item)
                    continue;
                QString expression = item->text().trimmed();
                if (expression.isEmpty())
                    continue;
                QComboBox *patternSyntax = qobject_cast<QComboBox*>(regexpTable->indexWidget(regexpTable->model()->index(i,1)));
                if (!patternSyntax)
                    continue;
                QRegExp regexp(expression, Qt::CaseSensitive, s_patterns.value(patternSyntax->currentText()));
                ss->regexpList.append(regexp);
            }
            ss->whitelist = whitelistComboBox->currentIndex();
        }
        ignoredPathList->setModel(nullptr);
        while (regexpTable->rowCount()>0) {
            regexpTable->removeRow(0);
        }

        if (volume.isEmpty()) {
            ignoredPathList->setEnabled(false);
            addIgnoredPath->setEnabled(false);
            removeIgnoredPath->setEnabled(false);
            regexpTable->setEnabled(false);
            addRegexpButton->setEnabled(false);
            removeRegexpButton->setEnabled(false);
            whitelistComboBox->setEnabled(false);
        }
        else {
            ignoredPathList->setEnabled(true);
            addIgnoredPath->setEnabled(true);
            removeIgnoredPath->setEnabled(false);
            regexpTable->setEnabled(true);
            addRegexpButton->setEnabled(true);
            removeRegexpButton->setEnabled(true);
            whitelistComboBox->setEnabled(true);

            auto ss = m_selectiveSync.value(volume);
            ignoredPathList->setModel(ss->model);
            whitelistComboBox->setCurrentIndex(ss->whitelist);
            foreach (auto regexp, ss->regexpList) {
                QString patternSyntax = s_patterns.key(regexp.patternSyntax(), "RegExp");
                addRegexp(regexp.pattern(), patternSyntax);
            }
        }
        currentVolume = volume;
    };

    foreach (const QString &vol, mConfig->volumes()) {
        SelectiveSyncStruct *ss = new SelectiveSyncStruct();
        ss->model = new QStringListModel();
        VolumeConfig volume = mConfig->volume(vol);
        ss->model->setStringList(volume.ignoredPaths());
        ss->whitelist = volume.whitelistMode();
        ss->regexpList = volume.regExpList();
        m_selectiveSync.insert(vol, ss);
    }

    QHeaderView* regexpHeaderView = new QHeaderView(Qt::Horizontal, regexpTable);
    regexpTable->setHorizontalHeader(regexpHeaderView);
    regexpHeaderView->setSectionResizeMode(0, QHeaderView::Stretch);
    regexpHeaderView->setSectionResizeMode(1, QHeaderView::Interactive);
    regexpTable->setColumnWidth(1, 150);

    connect(addRegexpButton, &QPushButton::clicked, [this] () { addRegexp(); });
    connect(removeRegexpButton, &QPushButton::clicked, this, &SettingsDialog::removeRegexp );
    connect(addIgnoredPath, SIGNAL(clicked()), this, SLOT(onAddIgnoredPath()));
    connect(removeIgnoredPath, SIGNAL(clicked()), this, SLOT(onRemoveIgnoredpath()));
    connect(ignoredPathList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(onPathSelected(const QModelIndex&)));

    loadVolume("");
    connect(comboBoxVolume, &QComboBox::currentTextChanged, loadVolume);
    comboBoxVolume->addItems(mConfig->volumes());
}

void SettingsDialog::setupProfileManagerPage()
{
    m_profile = mConfig->profile();
    if (m_profile.isEmpty())
        m_profile = "default";
    m_profilesInitialized = false;
    initializeProfilesManager();
}

void SettingsDialog::setupWarningsPage()
{
    WarningsTable *table = new WarningsTable(mSxState, this);
    pageWarnings->layout()->addWidget(table);
}

void SettingsDialog::setupLoggingPage()
{
    enableDebugLog->setChecked(mConfig->desktopConfig().debugLog());
    if (mConfig->desktopConfig().debugLog()) {
        comboBoxLogLevel->setEnabled(true);
    }
    else {
        comboBoxLogLevel->setEnabled(false);
    }
    comboBoxLogLevel->setCurrentIndex(mConfig->desktopConfig().logLevel());

    logView->setModel(LogsModel::instance());
    logView->resizeColumnToContents(0);
    logView->horizontalHeader()->setStretchLastSection(true);
    logView->scrollToBottom();
    m_scrolledToBottom = true;

    connect(logView->model(), &LogsModel::rowsAboutToBeInserted, this, &SettingsDialog::beforeLogInsert);
    connect(logView->model(), &LogsModel::rowsInserted, this, &SettingsDialog::onLogsInserted);
    connect(clearLogButton, &QPushButton::clicked, LogsModel::instance(), &LogsModel::removeLogs);
    connect(openLogButton, &QPushButton::clicked, this, &SettingsDialog::onOpenLog);

    if (isRetina()) {
        clearLogButton->setImage(":/buttons/logs_clear@2x.png");
        openLogButton->setImage(":/buttons/logs_opendir@2x.png");
        sendLogsButton->setImage(":/buttons/logs_export@2x.png");
        pushButton->setImage(":/buttons/logs_delete@2x.png");
    }
    else {
        clearLogButton->setImage(":/buttons/logs_clear.png");
        openLogButton->setImage(":/buttons/logs_opendir.png");
        sendLogsButton->setImage(":/buttons/logs_export.png");
        pushButton->setImage(":/buttons/logs_delete.png");
    }

    /*
    if (!config.debugLog())
        LogsModel::instance()->removeLogs();
    */
}

void SettingsDialog::onStateChanged(SxStatus status)
{
    /*
    QT_TRANSLATE_NOOP("ProfileStatus", "paused")}},
        {"idle",            {"#000000", QT_TRANSLATE_NOOP()}},
        {"working",         {"#49cb15", QT_TRANSLATE_NOOP()}},
        {"warning",         {"#f6ae00", QT_TRANSLATE_NOOP("ProfileStatus", "warning")}},
        {"inactive",        {"#ff0000", QT_TRANSLATE_NOOP("ProfileStatus", "")
*/
    QString stateStr;
    switch (status) {
    case SxStatus::idle : {
        stateStr = tr("Sync idle");
    } break;
    case SxStatus::inactive : {
        stateStr = tr("Sync not active");
    } break;
    case SxStatus::paused : {
        emit sig_paused();
        stateStr = tr("Sync paused");
    } break;
    case SxStatus::working : {
        stateStr = tr("Sync in progress");
    } break;
    }

    QString icon = ":/icons/pause";
    if (status == SxStatus::paused)
        icon+="-active";
    if (isRetina())
        icon+="@2x";
    icon += ".png";

    pauseButton->setImage(icon);
    stateLabel->setText(stateStr);
}

void SettingsDialog::onConfigPageChanged()
{
    auto selected = configPagesList->selectedItems();
    Q_ASSERT(selected.count() <= 1);
    if (selected.count() == 0)
    {
        configPagesList->setCurrentRow(m_currentPageIndex);
    }
}

void SettingsDialog::onConfigPageChanged(int row)
{
    m_currentPageIndex = row;
    if (m_currentPageIndex == SettingsPage::Volumes)
        emit sig_requestVolumeList();
}

void SettingsDialog::beforeLogInsert()
{
    const QScrollBar *sb = logView->verticalScrollBar();
    m_scrolledToBottom = (sb->value()==sb->maximum());
}

void SettingsDialog::onLogsInserted(const QModelIndex &, int first, int last)
{
    /*
    for (int i=first; i<=last; i++)
        logView->resizeRowToContents(i);
    */
    Q_UNUSED(first);
    Q_UNUSED(last);
    if (m_scrolledToBottom)
        logView->scrollToBottom();
}

void SettingsDialog::onAddIgnoredPath()
{
    QString volume = comboBoxVolume->currentText();
    if (volume.isEmpty())
        return;
    QString localPath = mConfig->volume(volume).localPath();
    auto dir = QFileDialog::getExistingDirectory(this, tr("Select ignored directory"), localPath);
    if (!dir.isEmpty()) {
        if (dir == localPath) {
            QMessageBox::warning(nullptr, tr("Cannot add selected path"), tr("Cannot add selected path: must be different than the local directory."));
            return;
        }
        try {
            dir = makeRelativeTo(localPath, dir);
        }
        catch (const std::logic_error&) {
            QMessageBox::warning(nullptr, tr("Cannot add selected path"), tr("Cannot add selected path. Must be in the selected local directory."));
            return;
        }

        // make sure it's unique
        QStringListModel *model = m_selectiveSync.value(volume)->model;
        for (int i = 0; i<model->rowCount(); i++) {
            auto idx = model->index(i, 0);
            if (model->data(idx, Qt::DisplayRole).toString() == dir)
                return;
        }
        model->insertRows(model->rowCount(), 1);

        auto const idx = model->index(model->rowCount() - 1, 0);
        ignoredPathList->setCurrentIndex(idx);
        model->setData(idx, QVariant(dir));
        removeIgnoredPath->setEnabled(true);
    }
}

void SettingsDialog::onRemoveIgnoredpath()
{
    auto currentModel = m_selectiveSync.value(comboBoxVolume->currentText())->model;
    auto const idx = ignoredPathList->currentIndex();
    currentModel->removeRow(idx.row());
    if (currentModel->rowCount() == 0) {
        removeIgnoredPath->setEnabled(false);
    }
}

void SettingsDialog::storeSettings()
{
    comboBoxVolume->clear();
    auto desktopConfig = mConfig->desktopConfig();
    bool checkUpdates = desktopConfig.checkUpdates();
    VersionCheck::instance()->setEnabled(checkNewVersion->isChecked());
    VersionCheck::instance()->setCheckingBeta(checkBetaVersion->isChecked());
    if (!checkUpdates && checkNewVersion->isChecked())
        VersionCheck::instance()->checkNow();

    desktopConfig.setAutostart(autoStart->isChecked());
    desktopConfig.setCheckUpdates(checkNewVersion->isChecked(), checkBetaVersion->isChecked());
    desktopConfig.setDebugLog(enableDebugLog->isChecked());
    desktopConfig.setLogLevel(comboBoxLogLevel->currentIndex());
    LogLevel logLevel = static_cast<LogLevel>(static_cast<int>(LogLevel::Info)-comboBoxLogLevel->currentIndex());
    SxLog::instance().setLogLevel(logLevel);

    QString lang = Translations::instance()->languageCode(languageSelector->currentText());
    desktopConfig.setLanguage(lang);
    desktopConfig.setNotifications(notifications->isChecked());

    desktopConfig.setTrayIconMark((comboBoxShape->currentIndex()==0) ? "" : comboBoxShape->currentText(), buttonColor->text());
    QPair<QString,QString> shapeDesc = desktopConfig.trayIconMark();
    emit sig_updateTrayShape(shapeDesc);

    bool volumesChanged = false;

    QHash<QString, QString> selectedVolumes = scrollArea->selectedVolumes();
    if (selectedVolumes.count() != mConfig->volumes().count())
        volumesChanged = true;
    else {
        foreach (QString volume, selectedVolumes.keys()) {
            if (!mConfig->volumes().contains(volume)) {
                volumesChanged = true;
                break;
            }
            QDir oldDir(mConfig->volume(volume).localPath());
            QDir newDir(selectedVolumes.value(volume));
            if (oldDir.absolutePath() != newDir.absolutePath()) {
                volumesChanged = true;
                break;
            }
            auto ss = m_selectiveSync.value(volume);
            if (ss->whitelist != mConfig->volume(volume).whitelistMode()) {
                volumesChanged = true;
                break;
            }
            if (ss->model->stringList() != mConfig->volume(volume).ignoredPaths()) {
                volumesChanged = true;
                break;
            }
            if (ss->regexpList != mConfig->volume(volume).regExpList()) {
                volumesChanged = true;
                break;
            }
        }
    }
    if (volumesChanged) {
        emit sig_setUserControllEnabled(false);

        QEventLoop loop;
        connect(this, &SettingsDialog::sig_paused, &loop, &QEventLoop::quit);
        emit sig_forcePause();
        loop.exec();

        foreach (QString volume, mConfig->volumes()) {
            if (!selectedVolumes.keys().contains(volume)) {
                mConfig->removeVolumeConfig(volume);
                SxDatabase::instance().removeVolumeFiles(volume);
                SxDatabase::instance().removeVolumeHistory(volume);
                removeVolumeFilterConfig(mConfig->clusterConfig().uuid(), volume);
            }
        }
        foreach (QString volume, selectedVolumes.keys()) {
            if (mConfig->volumes().contains(volume)) {
                if (mConfig->volume(volume).localPath() != selectedVolumes.value(volume)) {
                    SxDatabase::instance().removeVolumeFiles(volume);
                }
            }
            mConfig->addVolumeConfig(volume, selectedVolumes.value(volume));
            auto ss = m_selectiveSync.value(volume);
            if (ss) {
                mConfig->volume(volume).setSelectiveSync(ss->model->stringList(), ss->whitelist, ss->regexpList);
            }
        }
        mConfig->syncConfig();
        emit sig_setUserControllEnabled(true);
        emit sig_restartFilesystemWatcher();
        emit sig_pauseResume();
    }
}

void SettingsDialog::disableAutoUpdate()
{
    checkNewVersion->setEnabled(false);
    buttonCheckNow->setEnabled(false);
}

void SettingsDialog::setVCluster(const QString &vcluster)
{
    m_vcluster = vcluster;
}

void SettingsDialog::lockVolume(const QString &volume)
{
    scrollArea->lockVolume(volume);
}

void SettingsDialog::onVolumeNameChanged()
{
    mConfig->syncConfig();
    auto volumes = mConfig->volumes();
    updateVolumeList();
    comboBoxVolume->clear();
    foreach (auto ss, m_selectiveSync.values()) {
        delete ss;
    }
    m_selectiveSync.clear();
    foreach (const QString &vol, mConfig->volumes()) {
        SelectiveSyncStruct *ss = new SelectiveSyncStruct();
        ss->model = new QStringListModel();
        VolumeConfig volume = mConfig->volume(vol);
        ss->model->setStringList(volume.ignoredPaths());
        ss->whitelist = volume.whitelistMode();
        ss->regexpList = volume.regExpList();
        m_selectiveSync.insert(vol, ss);
    }
    comboBoxVolume->addItems(mConfig->volumes());
}

void SettingsDialog::onPathSelected(const QModelIndex&)
{
    removeIgnoredPath->setEnabled(true);
}

void SettingsDialog::on_buttonWizard_clicked()
{
    close();
    emit showWizard();
}

void SettingsDialog::on_buttonCheckNow_clicked()
{
    VersionCheck::instance()->resetVersionCheck();
    emit checkNowForNewVersion(checkBetaVersion->isChecked());
}

void SettingsDialog::initializeProfilesManager()
{
    if (m_profilesInitialized)
        return;

    m_profilesWidgets.clear();
    m_profilesInitialized = true;

    QSpacerItem *spacer = verticalSpacerProfilesManager;
    QGridLayout *layout = static_cast<QGridLayout*>(scrollAreaProfilesContent->layout());

    layout->removeItem(spacer);
    int index = 1;

    QStringList profiles = ProfileManager::instance()->listProfiles();
    foreach(QString p, profiles)
    {

        ProfilePtr profile = ProfilePtr::createEmptyWidgets();
        updateProfileWidgets(profile, ProfileManager::ProfileStatus(p, "...", false));

        connect(profile.settings, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileSettingsClicked);
        connect(profile.startStop, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileStartStopClicked);
        connect(profile.pauseResume, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfilePauseResumeClicked);
        connect(profile.removeProfile, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileRemoveClicked);

        m_profilesWidgets.append(profile);

        layout->addWidget(profile.name, index, 0);
        layout->addWidget(profile.status, index, 1);
        layout->addWidget(profile.settings, index, 2, Qt::AlignHCenter);
        layout->addWidget(profile.startStop, index, 3);
        layout->addWidget(profile.pauseResume, index, 4);
        layout->addWidget(profile.removeProfile, index, 5);

        index++;
    }
    layout->addItem(spacer, index, 0);

    mProfileUpdateTimer.setSingleShot(true);
    connect(&mProfileUpdateTimer, &QTimer::timeout, ProfileManager::instance(), &ProfileManager::requestProfilesStatus);
    connect(ProfileManager::instance(), &ProfileManager::gotProfilesStatus, this, &SettingsDialog::onUpdateProfiles);
    mProfileUpdateTimer.start(0);
}

void SettingsDialog::paintEvent(QPaintEvent *e)
{
    static bool firstPaint = true;
    if (firstPaint)
    {
        int heightDiff = frameGeometry().height()-height();
        setMaximumHeight(QApplication::desktop()->availableGeometry().height()-heightDiff);
        firstPaint=false;
    }
    QDialog::paintEvent(e);
}


void SettingsDialog::on_pushButton_clicked()
{
    int button = QMessageBox::question(this, __applicationName, tr("Do you want to delete all log files?"));
    if (button == QMessageBox::Yes)
        SxLog::instance().removeLogFiles();

}

void SettingsDialog::on_changePasswordButton_clicked()
{
    emit changePasswordClicked();
}

void SettingsDialog::on_checkNewVersion_stateChanged(int arg1)
{
    checkBetaVersion->setEnabled(arg1);
    if (!arg1)
        checkBetaVersion->setChecked(false);
}

void SettingsDialog::on_enableDebugLog_stateChanged(int arg1)
{
    comboBoxLogLevel->setEnabled(arg1);
}

QMap<QString, QRegExp::PatternSyntax> SettingsDialog::s_patterns = {
        { "RegExp", QRegExp::RegExp},
        { "RegExp2", QRegExp::RegExp2},
        { "Wildcard", QRegExp::Wildcard},
        { "WildcardUnix", QRegExp::WildcardUnix},
        { "FixedString", QRegExp::FixedString},
        { "W3CXmlSchema11", QRegExp::W3CXmlSchema11}
        };

void SettingsDialog::addRegexp(QString expression, QString syntax)
{
    int row = regexpTable->rowCount();
    regexpTable->insertRow(row);
    QComboBox *comboBox = new QComboBox();
    comboBox->addItems(s_patterns.keys());
    regexpTable->setIndexWidget(regexpTable->model()->index(row,1), comboBox);

    if (!expression.isEmpty())
    {
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setText(expression);
        regexpTable->setItem(row,0,item);
    }

    if (!syntax.isEmpty())
        comboBox->setCurrentText(syntax);
    else
        comboBox->setCurrentText("WildcardUnix");

}

void SettingsDialog::removeRegexp()
{
    int index = regexpTable->currentIndex().row();
    if (index < 0)
        return;

    int result = QMessageBox::question(this, __applicationName, tr("Do you want to remove selected expression?"));
    if (result == QMessageBox::Yes)
    {
        regexpTable->removeRow(index);
    }
}

static const QHash<QString, QPair<const char*, const char*>> status_info = {
    {"...",             {"#a0a0a0", QT_TRANSLATE_NOOP("ProfileStatus", "...")}},
    {"off",             {"#a0a0a0", QT_TRANSLATE_NOOP("ProfileStatus", "off")}},
    {"not responding",  {"#ff0000", QT_TRANSLATE_NOOP("ProfileStatus", "not responding")}},
    {"paused",          {"#97c9c5", QT_TRANSLATE_NOOP("ProfileStatus", "paused")}},
    {"paused-wizard",   {"#97c9c5", QT_TRANSLATE_NOOP("ProfileStatus", "paused")}},
    {"idle",            {"#000000", QT_TRANSLATE_NOOP("ProfileStatus", "idle")}},
    {"working",         {"#49cb15", QT_TRANSLATE_NOOP("ProfileStatus", "working")}},
    {"warning",         {"#f6ae00", QT_TRANSLATE_NOOP("ProfileStatus", "warning")}},
    {"inactive",        {"#ff0000", QT_TRANSLATE_NOOP("ProfileStatus", "inactive")}}
};

void SettingsDialog::updateProfileWidgets(SettingsDialog::ProfilePtr &widgets, const ProfileManager::ProfileStatus &status)
{
    if (!widgets.settings->property("profile").isValid())
    {
        widgets.settings->setProperty("profile", status.profileName);
        widgets.startStop->setProperty("profile", status.profileName);
        widgets.pauseResume->setProperty("profile", status.profileName);
        widgets.removeProfile->setProperty("profile", status.profileName);
    }
    QString oldStatus = widgets.status->property("status").toString();
    if (status.profileName=="default") {
        widgets.name->setText(tr("default"));
        widgets.name->setProperty("default", true);
    }
    else
        widgets.name->setText(status.profileName);

    widgets.status->setProperty("status", status.profileStatus);

    if (status_info.contains(status.profileStatus)) {
        auto pair = status_info[status.profileStatus];
        QString status_tr = QApplication::translate("ProfileStatus", pair.second);
        QString color = pair.first;
        widgets.status->setText(status_tr);
        widgets.status->setStyleSheet(QString("color: %1;").arg(color));
    }
    else {
        widgets.status->setText(status.profileStatus);
        widgets.status->setStyleSheet(QString());
    }

    if (oldStatus!=status.profileStatus && status.profileName != m_profile)
    {
        if (status.profileStatus == "...") {
            widgets.settings->setState(SxProfileManagerButton::state3);
            widgets.startStop->setState(SxProfileManagerButton::state3);
            widgets.pauseResume->setState(SxProfileManagerButton::state3);
            widgets.removeProfile->setState(SxProfileManagerButton::state3);
        }
        if (status.profileStatus == "off")
        {
            widgets.settings->setState(SxProfileManagerButton::state3);
            widgets.startStop->setState(SxProfileManagerButton::state1);
            widgets.pauseResume->setState(SxProfileManagerButton::state3);
            widgets.removeProfile->setState(SxProfileManagerButton::state1);
        }
        else if (status.profileStatus == "not responding")
        {
            widgets.settings->setState(SxProfileManagerButton::state3);
            widgets.startStop->setState(SxProfileManagerButton::state3);
            widgets.pauseResume->setState(SxProfileManagerButton::state3);
            widgets.removeProfile->setState(SxProfileManagerButton::state3);
        }
        else if (status.profileStatus == "paused")
        {
            widgets.settings->setState(SxProfileManagerButton::state1);
            widgets.startStop->setState(SxProfileManagerButton::state2);
            widgets.pauseResume->setState(SxProfileManagerButton::state2);
            widgets.removeProfile->setState(SxProfileManagerButton::state3);
        }
        else if (status.profileStatus == "paused-wizard")
        {
            widgets.settings->setState(SxProfileManagerButton::state3);
            widgets.startStop->setState(SxProfileManagerButton::state2);
            widgets.pauseResume->setState(SxProfileManagerButton::state3);
            widgets.removeProfile->setState(SxProfileManagerButton::state3);
        }
        else
        {
            widgets.settings->setState(SxProfileManagerButton::state1);
            widgets.startStop->setState(SxProfileManagerButton::state2);
            widgets.pauseResume->setState(SxProfileManagerButton::state1);
            widgets.removeProfile->setState(SxProfileManagerButton::state3);
        }
    }
}

void SettingsDialog::onUpdateProfiles(const QList<ProfileManager::ProfileStatus> &list)
{
    bool relayout = false;
    bool skipFirst = false;

    if (list.at(0).profileName=="default")
    {
        skipFirst = true;
        if (m_profilesWidgets.at(0).name->property("default").isValid())
        {
            updateProfileWidgets(m_profilesWidgets[0], list.at(0));
        }
        else
        {
            relayout = true;
            ProfilePtr profile = ProfilePtr::createEmptyWidgets();
            updateProfileWidgets(profile, list.at(0));
            m_profilesWidgets.insert(0, profile);

            connect(profile.settings, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileSettingsClicked);
            connect(profile.startStop, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileStartStopClicked);
            connect(profile.pauseResume, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfilePauseResumeClicked);
            connect(profile.removeProfile, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileRemoveClicked);
        }
    }
    else
    {
        if (m_profilesWidgets.at(0).name->property("default").isValid())
        {
            relayout=true;
            ProfilePtr p = m_profilesWidgets.takeFirst();
            p.settings->deleteLater();
            p.name->deleteLater();
            p.pauseResume->deleteLater();
            p.removeProfile->deleteLater();
            p.startStop->deleteLater();
            p.status->deleteLater();
        }
    }

    int secondIndex = skipFirst ? 1 : 0;
    for (int i=secondIndex ; i<list.count(); i++)
    {
        auto profileStatus = list.at(i);

        while (secondIndex < m_profilesWidgets.count() && profileStatus.profileName > m_profilesWidgets.at(secondIndex).name->text())
        {
            relayout = true;
            ProfilePtr p = m_profilesWidgets.takeAt(secondIndex);
            p.settings->deleteLater();
            p.name->deleteLater();
            p.pauseResume->deleteLater();
            p.removeProfile->deleteLater();
            p.startStop->deleteLater();
            p.status->deleteLater();
        }

        if (secondIndex < m_profilesWidgets.count())
        {
            if (profileStatus.profileName < m_profilesWidgets.at(secondIndex).name->text())
            {
                relayout = true;
                ProfilePtr profile = ProfilePtr::createEmptyWidgets();
                updateProfileWidgets(profile, profileStatus);
                m_profilesWidgets.insert(secondIndex, profile);

                connect(profile.settings, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileSettingsClicked);
                connect(profile.startStop, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileStartStopClicked);
                connect(profile.pauseResume, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfilePauseResumeClicked);
                connect(profile.removeProfile, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileRemoveClicked);

                secondIndex++;
            }
            if (profileStatus.profileName == m_profilesWidgets.at(secondIndex).name->text())
            {
                updateProfileWidgets(m_profilesWidgets[secondIndex], profileStatus);
                secondIndex++;
            }
        }
        else
        {
            relayout = true;
            ProfilePtr profile = ProfilePtr::createEmptyWidgets();
            updateProfileWidgets(profile, profileStatus);
            m_profilesWidgets.append(profile);

            connect(profile.settings, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileSettingsClicked);
            connect(profile.startStop, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileStartStopClicked);
            connect(profile.pauseResume, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfilePauseResumeClicked);
            connect(profile.removeProfile, &SxProfileManagerButton::clicked, this, &SettingsDialog::onProfileRemoveClicked);

            secondIndex++;
        }
    }
    while (secondIndex < m_profilesWidgets.count())
    {
        relayout = true;
        ProfilePtr p = m_profilesWidgets.takeLast();
        p.settings->deleteLater();
        p.name->deleteLater();
        p.pauseResume->deleteLater();
        p.removeProfile->deleteLater();
        p.startStop->deleteLater();
        p.status->deleteLater();
    }

    if (relayout)
    {
        QSpacerItem *spacer = verticalSpacerProfilesManager;
        QGridLayout *layout = static_cast<QGridLayout*>(scrollAreaProfilesContent->layout());
        layout->removeItem(spacer);

        foreach (ProfilePtr p, m_profilesWidgets) {
            layout->removeWidget(p.settings);
            layout->removeWidget(p.name);
            layout->removeWidget(p.pauseResume);
            layout->removeWidget(p.removeProfile);
            layout->removeWidget(p.startStop);
            layout->removeWidget(p.status);
        }
        int index = 1;
        foreach(ProfilePtr profile, m_profilesWidgets)
        {
            layout->addWidget(profile.name, index, 0, 1, 1);
            layout->addWidget(profile.status, index, 1, 1, 1);
            layout->addWidget(profile.settings, index, 2, 1, 1, Qt::AlignHCenter);
            layout->addWidget(profile.startStop, index, 3, 1, 1);
            layout->addWidget(profile.pauseResume, index, 4, 1, 1);
            layout->addWidget(profile.removeProfile, index, 5, 1, 1);

            index++;
        }
        layout->addItem(spacer, index, 0);
    }
#ifdef Q_OS_WIN
    mProfileUpdateTimer.start(1000);
#else
    mProfileUpdateTimer.start(500);
#endif
}

void SettingsDialog::onProfileSettingsClicked()
{
    SxProfileManagerButton *button = qobject_cast<SxProfileManagerButton*>(sender());
    if (button && button->property("profile").isValid()
            && button->getState() != SxProfileManagerButton::state3)
    {
        QString profile = button->property("profile").toString();
        QStringList arguments = {"--profile", profile, "--open-settings"};
        QProcess::execute(QApplication::applicationFilePath(), arguments);
    }
}

void SettingsDialog::onProfileStartStopClicked()
{
    SxProfileManagerButton *button = qobject_cast<SxProfileManagerButton*>(sender());
    if (button && button->property("profile").isValid()
            && button->getState() != SxProfileManagerButton::state3)
    {
        bool start = button->getState() == SxProfileManagerButton::state1;
        button->setState(SxProfileManagerButton::state3);
        QString profile = button->property("profile").toString();
        QStringList arguments = {"--profile", profile, start?"--start":"--close"};
        QProcess::execute(QApplication::applicationFilePath(), arguments);
    }
}

void SettingsDialog::onProfilePauseResumeClicked()
{
    SxProfileManagerButton *button = qobject_cast<SxProfileManagerButton*>(sender());
    if (button && button->property("profile").isValid()
            && button->getState() != SxProfileManagerButton::state3)
    {
        bool pause = button->getState() == SxProfileManagerButton::state1;
        button->setState(SxProfileManagerButton::state3);
        QString profile = button->property("profile").toString();
        QStringList arguments = {"--profile", profile, pause?"--pause":"--resume"};
        QProcess::execute(QApplication::applicationFilePath(), arguments);
    }
}

void SettingsDialog::onProfileRemoveClicked()
{
    SxProfileManagerButton *button = qobject_cast<SxProfileManagerButton*>(sender());
    if (button && button->property("profile").isValid()
            && button->getState() != SxProfileManagerButton::state3)
    {
        QString profile = button->property("profile").toString();
        QString profileMsg = profile;
        if (profile == "default")
            profileMsg = tr("default");
        int result = QMessageBox::question(this, __applicationName, tr("Do you want to remove profile %1").arg(profileMsg));
        if (result == QMessageBox::Yes)
        {
            button->setState(SxProfileManagerButton::state3);
            auto status = ProfileManager::instance()->profileStatus(profile);
            if (status.first() != "off") {
                QMessageBox::warning(this, __applicationName, tr("Profile %1 should be closed first").arg(profileMsg));
                return;
            }
            if (!ProfileManager::instance()->removeProfile(profile))
                QMessageBox::warning(this, __applicationName, tr("Removing profile %1 failed").arg(profileMsg));
        }
    }
}



SettingsDialog::ProfilePtr SettingsDialog::ProfilePtr::createEmptyWidgets()
{
    QString ext = isRetina() ? "@2x.png" : ".png";
    ProfilePtr profile;
    profile.name = new QLabel();
    profile.status = new QLabel();
    profile.settings = new SxProfileManagerButton(":/buttons/pm_settings"+ext, "", ":/buttons/pm_settings_inactive"+ext, SxProfileManagerButton::state3);
    profile.startStop = new SxProfileManagerButton(":/buttons/pm_start"+ext, ":/buttons/pm_stop"+ext, ":/buttons/pm_start_inactive"+ext, SxProfileManagerButton::state3);
    profile.pauseResume = new SxProfileManagerButton(":/buttons/pm_pause"+ext, ":/buttons/pm_resume"+ext, ":/buttons/pm_pause_inactive"+ext, SxProfileManagerButton::state3);
    profile.removeProfile = new SxProfileManagerButton(":/buttons/pm_remove"+ext, "", ":/buttons/pm_remove_inactive"+ext, SxProfileManagerButton::state3);
    return profile;
}

void SettingsDialog::on_pushButton_2_clicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, __applicationName, tr("Enter new profile name"), QLineEdit::Normal, QString(), &ok);
    if (ok)
    {
        name = name.trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(this, __applicationName, tr("Profile name cannot be empty"));
            return;
        }
        if (name.contains(" "))
        {
            QMessageBox::warning(this, __applicationName, tr("Profile name cannot contains spaces"));
            return;
        }

        QStringList profiles = ProfileManager::instance()->listProfiles();
#if defined Q_OS_WIN || defined Q_OS_MAC
        Qt::CaseSensitivity caseSensitive = Qt::CaseInsensitive;
#else
        Qt::CaseSensitivity caseSensitive = Qt::CaseSensitive;
#endif
        if (profiles.contains(name, caseSensitive)) {
            QMessageBox::warning(this, __applicationName, tr("Profile %1 already exists").arg(name));
            return;
        }
        ProfileManager::instance()->createProfile(name);
    }
}

void SettingsDialog::on_sendLogsButton_clicked()
{
#ifdef Q_OS_OSX
    QString filename = QString("%1-logs-%2.log").arg(__applicationName.toLower()).arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmm"));
#else
    QString filename = QString("%1-logs-%2.log.gz").arg(__applicationName.toLower()).arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmm"));
#endif

    QFileDialog dialog(this);
    dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    dialog.selectFile(dialog.directory().filePath(filename));
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
#ifdef Q_OS_OSX
    dialog.setNameFilter("*.gz");
#else
    dialog.setNameFilter("*.log.gz");
#endif

    if (dialog.exec()) {
        if (!SxLog::instance().exportLogs(dialog.selectedFiles().first()))
            QMessageBox::warning(this, __applicationName, "Exporting logs failed");
    }
}

void SettingsDialog::on_historyListView_doubleClicked(const QModelIndex &index)
{
    SyncHistoryModel *model = qobject_cast<SyncHistoryModel *>(historyListView->model());
    if (!model)
        return;
    QString historyFile = model->data(index, SyncHistoryModel::PathRole).toString();
    int sepIndex = historyFile.indexOf("/");
    QString volume = historyFile.mid(0, sepIndex);
    QString file = historyFile.mid(sepIndex);

    if (!mConfig->volumes().contains(volume))
        return;
    QString path = mConfig->volume(volume).localPath()+file;
    QFileInfo fileInfo(path);
    if (fileInfo.dir().exists()) {
        const QString uri = "file:///" + fileInfo.dir().absolutePath()+"/";
        qDebug() << CLASS_NAME << "Opening:" << uri;
        QDesktopServices::openUrl(QUrl(uri, QUrl::TolerantMode));
    }
}

void SettingsDialog::onOpenLog()
{
    QDir logDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/log";
    if (logDir.exists()) {
        bool is_empty = logDir.entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty();
        if (is_empty)
            QMessageBox::information(this, __applicationName, tr("Log directory is empty"));
        else {
            const QString uri = "file:///" + logDir.absolutePath() + "/";
            QDesktopServices::openUrl(QUrl(uri, QUrl::TolerantMode));
        }
    }
    else
        QMessageBox::warning(this, __applicationName, tr("Log directory doesn't exist"));
}

void SettingsDialog::on_clearWarningsButton_clicked()
{
    emit sig_clearWarnings();
}
