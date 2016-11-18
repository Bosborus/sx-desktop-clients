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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QListView>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QTreeView>
#include "util.h"
#include <sxlog.h>
#include <QDesktopServices>
#include <QStandardPaths>
#include "getpassworddialog.h"
#include "settingsdialog.h"
#include "scoutwizard.h"
#include "versioncheck.h"
#include "progressdialog.h"
#include "openingfiledialog.h"

QPixmap MainWindow::iconForMimeType(QString mimeType, bool smallImage) {
    static const QHash<QString, QString> mimeTypeIcon = {
        {"Volume",      ":/remoteBrowser/volume"},
        {"Volume/Locked",      ":/remoteBrowser/volume_locked"},
        {"Volume/Unlocked",      ":/remoteBrowser/volume_unlocked"},
        {"Volume/Unsupported",      ":/remoteBrowser/volume_unsupported"},
        {"Directory",   ":/remoteBrowser/directory"},
        {"File",        ":/remoteBrowser/file"},
        {"File/archive",        ":/remoteBrowser/file_archive"},
        {"File/code",        ":/remoteBrowser/file_code"},
        {"File/executable",        ":/remoteBrowser/file_exe"},
        {"File/image",        ":/remoteBrowser/file_img"},
        {"File/application",        ":/remoteBrowser/file_installer"},
        {"File/music",        ":/remoteBrowser/file_music"},
        {"File/pdf",        ":/remoteBrowser/file_pdf"},
        {"File/presentation",        ":/remoteBrowser/file_presentation"},
        {"File/spreadsheet",        ":/remoteBrowser/file_spreadsheet"},
        {"File/text",        ":/remoteBrowser/file_text"},
        {"File/video",        ":/remoteBrowser/file_video"},
    };
    QString pixmapName = mimeTypeIcon.value(mimeType);
    if (pixmapName.isEmpty())
        return QPixmap(":/remoteBrowser/no_icon.png");
    if (smallImage)
        pixmapName+="_small";
    pixmapName+=".png";

    QPixmap pixmap(pixmapName);
    if (pixmap.isNull())
        return QPixmap(":/remoteBrowser/no_icon.png");
    return pixmap;
}

void MainWindow::reloadConfig(ScoutQueue *queue)
{
    mQueue = queue;
    mModel->reloadClusterConfig(queue);
}

void MainWindow::showError(const QString &errorMessage)
{
    ui->errorArea->setVisible(true);
    ui->buttonCloseErrorArea->setVisible(true);
    ui->errorLabel->setText(errorMessage);
}

MainWindow::MainWindow(ScoutConfig *config, ScoutQueue *queue, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationName());
    mConfig = config;
    mModel = new ScoutModel(config->clusterConfig(), queue, this);
    mQueue = queue;

    if (mModel->cluster() == nullptr) {
        ui->filesView->setVisible(false);
        ui->errorLabel->setText("Unable to initialize cluster connection.\n"+mModel->lastError());
        connect(mModel, &ScoutModel::clusterInitialized, this, &MainWindow::clusterInitialized);
        ui->buttonCloseErrorArea->setVisible(false);
    }
    else {
        ui->errorArea->setVisible(false);
        connect(mModel, &ScoutModel::sigError, this, &MainWindow::showError);
    }

    connect(mModel, &ScoutModel::setViewEnabled, ui->filesView, &QTableView::setEnabled);
    connect(mModel, &ScoutModel::configReloaded, this, &MainWindow::refreshActionsAndTitle);

    connect(queue, &ScoutQueue::startTask, this, &MainWindow::queueStarted);
    connect(queue, &ScoutQueue::finished, this, &MainWindow::queueFinished);

    ui->volumesView->setModel(mModel);
    ui->volumesView->setRootIndex(mModel->volumesIndex());
    ui->volumesView->setItemDelegate(new VolumeViewDelegate(mModel, this));
    ui->volumesView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->volumesView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    ui->filesView->setConfig(mConfig);
    ui->filesView->setModel(mModel);
    ui->filesView->setRootIndex(mModel->filesIndex());
    ui->filesView->horizontalHeader()->setDefaultSectionSize(150);
    auto filesViewItemDelegate = new FileViewDelegate(mModel, ui->filesView, this);
    connect(filesViewItemDelegate, &FileViewDelegate::setRowHeight, ui->filesView, &FilesTableView::resizeRowHeight);
    ui->filesView->setItemDelegate(filesViewItemDelegate);
    ui->filesView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->filesView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    mModel->reloadVolumes();

    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);

    connect(ui->splitter, &QSplitter::splitterMoved, this, &MainWindow::recalculateColumnCount);
    QTimer::singleShot(0, this, SLOT(recalculateColumnCount()));

    connect(ui->volumesView, &QTableView::clicked, this, &MainWindow::onVolumeClicked);
    connect(ui->filesView, &FilesTableView::openIndex, this, &MainWindow::openIndex);
    connect(ui->filesView, &FilesTableView::signalAction, this, &MainWindow::onAction);
    refreshActionsAndTitle();

    ui->actionUpload->setEnabled(false);
    connect(ui->filesView, &FilesTableView::showTaskDialog, this, &MainWindow::showTaskDialog);
    connect(VersionCheck::instance(), &VersionCheck::newVersionAvailable, this, &MainWindow::onNewVersionAvailable);
    VersionCheck::instance()->setParentWidget(this);
    QTimer::singleShot(0, VersionCheck::instance(), SLOT(checkNow()));
    showTasksWarning(mQueue->hasFailedTasks());
    connect(mQueue, &ScoutQueue::sigShowWarning, this, &MainWindow::showTasksWarning);

    ui->filesView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->filesView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    ui->filesView->verticalScrollBar()->setSingleStep(10);

}

MainWindow::~MainWindow()
{
    VersionCheck::instance()->setParentWidget(nullptr);
    delete mModel;
    delete ui;
}

void MainWindow::refreshActionsAndTitle()
{
    bool canMovePrev = mModel->canMovePrev();
    bool canMoveNext = mModel->canMoveNext();
    bool canMoveUp = mModel->canMoveUp();
    bool canUpload = !mModel->currentVolume().isEmpty();
    ui->actionBack->setEnabled(canMovePrev);
    ui->actionNext->setEnabled(canMoveNext);
    ui->actionUp->setEnabled(canMoveUp);
    ui->actionUpload->setEnabled(canUpload);
    ui->actionTasks->setEnabled(mQueue->isWorking());
    if (mModel->currentVolume().isEmpty()) {
        setWindowTitle(QApplication::applicationName());
        ui->volumesView->clearSelection();
    }
    else {
        QString path = mModel->currentVolume()+mModel->currentPath();
        setWindowTitle(path.mid(0, path.length()-1));
        int volumeIndex = mModel->currentVolumeIndex();
        ui->volumesView->selectionModel()->select(mModel->index(volumeIndex, 0, mModel->volumesIndex()), QItemSelectionModel::SelectionFlag::ClearAndSelect);
    }
}

bool MainWindow::unlockVolume(const QString &volume)
{
    auto cluster = mModel->cluster();
    if (cluster == nullptr)
        return false;
    auto sxvolume = cluster->getSxVolume(volume);
    if (sxvolume == nullptr)
        return false;
    auto encryptionType = GetPasswordDialog::getVolumeEncryptionType(sxvolume);
    GetPasswordDialog dialog(cluster->auth(), cluster->uuid(), volume, encryptionType, this);
    return dialog.exec();
}

void MainWindow::queueFinished()
{
    ui->actionTasks->setEnabled(false);
}

void MainWindow::clusterInitialized()
{
    ui->filesView->setVisible(true);
    ui->errorArea->setVisible(false);
    connect(mModel, &ScoutModel::sigError, this, &MainWindow::showError);
    recalculateColumnCount();
}

void MainWindow::onAction(const QString &actionName)
{
    if (actionName == "upload") {
        on_actionUpload_triggered();
        return;
    }
    if (actionName == "up") {
        if (mModel->canMoveUp())
            on_actionUp_triggered();
    }
    else if (actionName == "next") {
        if (mModel->canMoveNext())
            on_actionNext_triggered();
    }
    else if (actionName == "back") {
        if (mModel->canMovePrev())
            on_actionBack_triggered();
    }
    else if (actionName == "refresh") {
        on_actionRefresh_triggered();
    }
    ui->filesView->setFocus();
}

void MainWindow::showTasksWarning(bool showWarning)
{
    static QIcon defaultIcon = ui->actionTasks->icon();
    static QIcon warningIcon = QIcon(":/remoteBrowser/tasks_warning_on.png");
    ui->actionTasks->setIcon(showWarning ? warningIcon : defaultIcon);
}

void MainWindow::recalculateColumnCount()
{
    int viewWidth = ui->filesView->width();
    int itemWidth = 150;
    int columnCount = viewWidth / itemWidth;
    if (columnCount <= 0)
        columnCount = 1;
    if (columnCount == mModel->columnCount(mModel->filesIndex()))
        return;
    auto index2d = ui->filesView->selectionModel()->selectedIndexes();
    auto index1d = mModel->mapSelectionFrom2D(index2d);
    mModel->setFilesColumnCount(columnCount);
    ui->filesView->setRowCount(mModel->rowCount(mModel->filesIndex()));
    index2d = mModel->create2Dselection(index1d);
    QDateTime time = QDateTime::currentDateTime();
    QItemSelection selection;
    foreach (auto index, index2d) {
        selection.select(index, index);
    }
    ui->filesView->clearSelection();
    ui->filesView->selectionModel()->select(selection, QItemSelectionModel::Select);
}

void MainWindow::onVolumeClicked(const QModelIndex &index)
{
    QString volume = index.data(ScoutModel::NameRole).toString();
    QString mimeType = index.data(ScoutModel::MimeTypeRole).toString();
    if (mimeType == "Volume" || mimeType == "Volume/Unlocked") {
        mModel->move(volume, "/");
        ui->filesView->clearSelection();
        refreshActionsAndTitle();
    }
    else if (mimeType == "Volume/Locked") {
        if (unlockVolume(volume))
            mModel->move(volume, "/");
    }
}

void MainWindow::openIndex(const QModelIndex &index)
{
    QString mimeType = mModel->data(index, ScoutModel::MimeTypeRole).toString();
    if (mimeType.isEmpty())
        return;
    if (mimeType == "Volume" || mimeType == "Volume/Unlocked") {
        QString volume = mModel->data(index, ScoutModel::NameRole).toString();
        ui->filesView->clearSelection();
        mModel->move(volume, "/");
        mLastDirDoubleClickedTime = QDateTime::currentDateTime();
    }
    else if (mimeType == "Volume/Locked") {
        QString volume = mModel->data(index, ScoutModel::NameRole).toString();
        if (unlockVolume(volume))
            mModel->move(volume, "/");
    }
    else if (mimeType == "Directory") {
        auto volume = mModel->currentVolume();
        auto path = mModel->currentPath();
        QString name = mModel->data(index, ScoutModel::NameRole).toString() + "/";
        QString tmp = path+name;
        ui->filesView->clearSelection();
        mModel->move(volume, tmp);
        mLastDirDoubleClickedTime = QDateTime::currentDateTime();
    }
    else if (mimeType.startsWith("File")) {
        auto volume = mModel->currentVolume();
        auto path = mModel->data(index, ScoutModel::FullPathRole).toString();
        auto size = mModel->data(index, ScoutModel::SizeRole).toLongLong();
        QString destination;
        if (!mConfig->cacheEnabled() || size > mConfig->cacheFileLimit() || size > mConfig->cacheSize()) {
            QString localDir = ui->filesView->getSavePath();
            if (localDir.isEmpty())
                return;
            destination = localDir;
        }
        OpeningFileDialog d(mConfig->clusterConfig(), this);
        QString file = d.downloadFile(volume, path, destination, mConfig->cacheSize());
        if (file.isEmpty())
            return;
        qDebug() << QDesktopServices::openUrl(QUrl("file:///" + file, QUrl::TolerantMode));
    }
    refreshActionsAndTitle();
    ui->filesView->setFocus();
    ui->filesView->setRowCount(mModel->rowCount(mModel->filesIndex()));
}


void MainWindow::onNewVersionAvailable(const QString &version)
{
#ifdef Q_OS_LINUX
    auto msg = tr("A new version (<b>%2</b>) of %1 is available.<br>").arg(QApplication::applicationName()).arg(version);
    QMessageBox::information(this, QApplication::applicationName(), msg);
#else
    auto msg = tr("A new version (<b>%2</b>) of %1 is available.<br>").arg(QApplication::applicationName()).arg(version)+tr("Do you want to upgrade now?");
    if (QMessageBox::question(this, QApplication::applicationName(), msg) == QMessageBox::Yes)
        VersionCheck::instance()->updateNow();
#endif
}

void MainWindow::queueStarted()
{
    ui->actionTasks->setEnabled(true);
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    recalculateColumnCount();
}

void MainWindow::closeEvent(QCloseEvent *)
{
    emit windowClosed(this);
    deleteLater();
}

VolumeViewDelegate::VolumeViewDelegate(ScoutModel *model, QObject *parent) : QItemDelegate(parent)
{
    mModel = model;
}

void VolumeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setFont(option.font);
    painter->setPen(Qt::black);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->setClipRect(option.rect);

    int mItemWidth = option.rect.width();

    QString mimeType = index.data(ScoutModel::MimeTypeRole).toString();
    QPixmap cMimeIcon = MainWindow::iconForMimeType(mimeType, true);
    QString name = mModel->data(index, ScoutModel::NameRole).toString();

    if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
        QFontMetrics fm(option.font);
        painter->fillRect(option.rect, QColor("#308cc6"));
        painter->setPen(Qt::white);
        const int cProgressWidth = mItemWidth - 3*mMargin - mIconSize;

        qint64 size = mModel->data(index, ScoutModel::SizeRole).toLongLong();
        qint64 sizeUsed = mModel->data(index, ScoutModel::SizeUsedRole).toLongLong();
        QString sizeStr = formatSize(size, 2);
        int usedWidth = size == 0 ? cProgressWidth : static_cast<int>(sizeUsed*cProgressWidth/size);
        int nameWidth = fm.width(name);
        int sizeWidth = fm.width(sizeStr);
        bool split = (nameWidth+sizeWidth+mIconSize+5*mMargin > option.rect.width());

        QRect iconRect = option.rect;
        iconRect.translate(mMargin, split ? (option.rect.height()-mIconSize)/2 : mMargin);
        iconRect.setSize({mIconSize, mIconSize});
        painter->drawPixmap(iconRect, cMimeIcon);

        QRect nameRect = option.rect;
        nameRect.translate(2*mMargin+mIconSize, mMargin*2);
        nameRect.setSize({split ? mItemWidth-mIconSize-3*mMargin : mItemWidth-mIconSize-sizeWidth-4*mMargin, fm.height()});
        painter->drawText(nameRect, fm.elidedText(name, Qt::ElideRight, nameRect.width()));

        QRect sizeStrRect = option.rect;
        if (split)  {
            sizeStrRect.translate(2*mMargin+mIconSize, mMargin*3+fm.height());
            sizeStrRect.setSize({mItemWidth-mIconSize-3*mMargin, nameRect.height()});
        }
        else {
            sizeStrRect.translate(mMargin*3+mIconSize+nameRect.width(), mMargin*2);
            sizeStrRect.setSize({sizeWidth+mMargin, nameRect.height()});
        }
        painter->drawText(sizeStrRect, Qt::AlignLeft, fm.elidedText(sizeStr, Qt::ElideRight, sizeStrRect.width()));

        QRect progressRect = option.rect;
        progressRect.translate(2*mMargin+iconRect.width(), split ? 4*mMargin+nameRect.height()+fm.height() : 3*mMargin+nameRect.height());
        progressRect.setSize({cProgressWidth, mProgressHeight});
        painter->drawRect(progressRect);
        progressRect.setSize({usedWidth, mProgressHeight});
        painter->fillRect(progressRect, Qt::white);
    }
    else {
        QFontMetrics fm(option.font);
        QRect iconRect = option.rect;
        iconRect.translate(mMargin, mMargin);
        iconRect.setSize({mIconSize, mIconSize});
        painter->drawPixmap(iconRect, cMimeIcon);

        QRect nameRect = option.rect;
        nameRect.translate(2*mMargin+mIconSize, mMargin);
        nameRect.setSize({mItemWidth-mIconSize-3*mMargin, mIconSize});
        painter->drawText(nameRect, Qt::AlignVCenter, fm.elidedText(name, Qt::ElideRight, nameRect.width()));
    }

    painter->restore();
}

QSize VolumeViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)

    int width = option.rect.width();
    int height = mItemHeight;
    QString name = index.data(ScoutModel::NameRole).toString();

    if (name == mModel->currentVolume()) {
        QFontMetrics fm(option.font);
        QString size = formatSize(index.data(ScoutModel::SizeRole).toLongLong(), 2);
        auto nameWidth = fm.width(name);
        auto sizeWidth = fm.width(size);
        if (nameWidth+sizeWidth+mIconSize+5*mMargin > width)
            height = 2*fm.height()+6*mMargin+mProgressHeight;
    }
    return QSize(width, height);
}

void MainWindow::on_actionHome_triggered()
{
    ui->filesView->clearSelection();
    ui->filesView->clearCurrentIndex();
    ui->volumesView->clearSelection();
    mModel->move("","");
    refreshActionsAndTitle();
}

void MainWindow::on_actionBack_triggered()
{
    ui->filesView->clearSelection();
    ui->filesView->clearCurrentIndex();
    mModel->movePrev();
    refreshActionsAndTitle();
}

void MainWindow::on_actionNext_triggered()
{
    ui->filesView->clearSelection();
    ui->filesView->clearCurrentIndex();
    mModel->moveNext();
    refreshActionsAndTitle();
}

void MainWindow::on_actionUp_triggered()
{
    ui->filesView->clearSelection();
    ui->filesView->clearCurrentIndex();
    mModel->moveUp();
    refreshActionsAndTitle();
}

void MainWindow::on_actionTasks_triggered()
{
    emit showHideTasksDialogClicked();
}

void MainWindow::on_actionUpload_triggered()
{
    static QString lastLocation = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QFileDialog dlg(this);
    dlg.setFileMode(QFileDialog::ExistingFiles);
    dlg.setOption(QFileDialog::DontUseNativeDialog,true);
    dlg.setDirectory(lastLocation);
    dlg.setOption(QFileDialog::DontUseNativeDialog,true);

    connect(&dlg, &QFileDialog::currentChanged, [&dlg](const QString &path) {
        QFileInfo finfo(path);
        if (finfo.isDir())
            dlg.setFileMode(QFileDialog::Directory);
        else
            dlg.setFileMode(QFileDialog::ExistingFiles);
    });

    if (!dlg.exec())
        return;
    int index = dlg.selectedFiles().first().lastIndexOf('/');
    lastLocation = dlg.selectedFiles().first().mid(0, index);
    QStringList paths;

    foreach (QString file, dlg.selectedFiles()) {
        QString path = file;
        QFileInfo fileInfo(path);
        if (fileInfo.isDir() || fileInfo.isBundle())
            path+="/";
        paths.append(path.mid(lastLocation.length()+1));
    }
    mModel->requestUpload(mModel->currentVolume(), lastLocation, paths, mModel->currentPath());
    showTaskDialog();
}

void MainWindow::on_actionRefresh_triggered()
{
    ui->filesView->clearSelection();
    mModel->refresh();
}

void MainWindow::on_actionSettings_triggered()
{
    SettingsDialog dialog(mConfig, this);
    dialog.setModal(true);
    connect(&dialog, &SettingsDialog::showWizard, [this](){
        ScoutWizard wizard(mConfig, this);
        wizard.setModal(true);
        wizard.exec();
        if (wizard.configChanged()) {
            emit clusterConfigChanged();
        }
    });
    dialog.exec();
}

void MainWindow::on_buttonCloseErrorArea_clicked()
{
    ui->errorArea->setVisible(false);
}
