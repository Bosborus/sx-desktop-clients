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

#include "tasksdialog.h"
#include "ui_tasksdialog.h"
#include <QTimer>
#include <QPainter>
#include <QBitmap>
#include <QMouseEvent>
#include "util.h"
#include "scoutmodel.h"

TasksDialog::TasksDialog(ScoutQueue *queue, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TasksDialog)
{
    mItemDelegate = new TaskDelegate(queue);
    ui->setupUi(this);
    ui->listView->setVisible(false);
    ui->toolButton->setText(tr("Show queue"));
    ui->listView->setItemDelegate(mItemDelegate);
    ui->listView->setModel(queue);
    ui->listView->setRootIndex(queue->tasksIndex());
    setTaskName("");
    setErrorText("");
    adjustSize();
    mQueue = queue;

    connect(mQueue, &QAbstractItemModel::dataChanged, this, &TasksDialog::onDataChanged);
    connect(mQueue, &QAbstractItemModel::rowsInserted, this, &TasksDialog::updateCountLabel);
    connect(mQueue, &QAbstractItemModel::rowsRemoved, this, &TasksDialog::updateCountLabel);
    connect(mQueue, &ScoutQueue::finished, this, &QDialog::close);

    updateCountLabel(mQueue->tasksIndex());
    QTimer::singleShot(0, this, SLOT(refreshCurrentTask()));

    QFontMetrics fm(ui->buttonCancel->font());
    int width = qMax(fm.width(ui->buttonCancel->text()), fm.width(ui->buttonRetry->text())) + 20;
    ui->buttonCancel->setFixedWidth(width);
    ui->buttonRetry->setFixedWidth(width);
    width = qMax(fm.width(tr("Hide queue")), fm.width(tr("Show queue"))) + 20;
    ui->toolButton->setFixedWidth(width);
}

TasksDialog::~TasksDialog()
{
    delete ui;
}

void TasksDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    refreshLabels();
}

void TasksDialog::on_toolButton_clicked()
{
    ui->listView->setVisible(!ui->listView->isVisible());
    int w = width();
    adjustSize();
    auto rect = geometry();
    rect.setWidth(w);
    setGeometry(rect);
    if (ui->listView->isVisible())
        ui->toolButton->setText(tr("Hide queue"));
    else
        ui->toolButton->setText(tr("Show queue"));
}

void TasksDialog::refreshCurrentTask()
{
    QString direction = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::DirectionRole).toString();
    QString error = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::ErrorRole).toString();;
    if (error.isEmpty()) {
        ui->buttonRetry->setVisible(false);
        ui->progressBar->setVisible(true);
        setErrorText("");
    }
    else {
        ui->buttonRetry->setVisible(true);
        ui->progressBar->setVisible(false);
        setErrorText(error);
    }
    QString name = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::TitleRole).toString();
    if (direction.isEmpty()) {
        setTaskName("...");
        ui->progressBar->setValue(0);
        ui->progressBar->setMaximum(0);
    }
    else {
        if (direction == "upload") {
            ui->labelDirection->setPixmap(QPixmap(":/remoteBrowser/task_upload.png"));
            setTaskName(QString("uploading %1").arg(name));
        }
        else {
            ui->labelDirection->setPixmap(QPixmap(":/remoteBrowser/task_download.png"));
            setTaskName(QString("downloading %1").arg(name));
        }
        qint64 size = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::SizeRole).toLongLong();
        qint64 done = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::ProgressRole).toLongLong();
        if (size == 0) {
            ui->progressBar->setValue(0);
            ui->progressBar->setMaximum(0);
        }
        else {
            qint64 value = 1000*done/size;
            ui->progressBar->setValue(static_cast<int>(value));
            ui->progressBar->setMaximum(1000);
        }
    }
}

TaskDelegate::TaskDelegate(ScoutQueue *model, QObject *parent)
    : QAbstractItemDelegate (parent)
{
    mQueue = model;
}

void TaskDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setFont(option.font);
    painter->setPen(Qt::black);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    QString name = index.data(ScoutQueue::TitleRole).toString();
    QString error = index.data(ScoutQueue::ErrorRole).toString();
    QString direction = index.data(ScoutQueue::DirectionRole).toString();
    qint64 size = index.data(ScoutQueue::SizeRole).toLongLong();

    int iconSize = 32;
    int retryButtonSpace = 0;
    if (!error.isEmpty()) {
        painter->fillRect(option.rect, Qt::gray);
        retryButtonSpace = 4+iconSize;
    }
    QRect iconRect = option.rect;
    iconRect.translate(4,4);
    iconRect.setSize({iconSize, iconSize});
    QPixmap icon(direction == "upload" ? ":/remoteBrowser/task_upload_small.png" : ":/remoteBrowser/task_download_small.png");
    painter->drawPixmap(iconRect, icon);

    int nameWidth = option.rect.width()-16-iconSize*2-retryButtonSpace;
    QRect nameRect = option.rect;
    nameRect.translate(8+iconSize, 4);
    nameRect.setSize({nameWidth, 15});

    QFontMetrics fm(option.font);
    QString title = fm.elidedText(name, Qt::ElideRight, nameWidth);

    painter->drawText(nameRect, title);
    nameRect.translate(0, 4+nameRect.height());
    painter->drawText(nameRect, formatSize(size, 2));

    if (!error.isEmpty()) {
        iconRect = option.rect;
        iconRect.translate(12+iconSize+nameWidth, 4);
        iconRect.setSize({iconSize, iconSize});
        auto mousePos = option.widget->mapFromGlobal(QCursor::pos());
        QPixmap cancelPixmap(":/remoteBrowser/refresh_normal_on.png");
        if (iconRect.contains(mousePos, true)) {
            auto mask = cancelPixmap.createMaskFromColor(Qt::transparent);
            cancelPixmap.fill(Qt::red);
            cancelPixmap.setMask(mask);
            mActiveRetryRectGlobal.setTopLeft(option.widget->mapToGlobal(iconRect.topLeft()));
            mActiveRetryRectGlobal.setSize(iconRect.size());
            mActiveCancelIndex = index;
        }
        painter->drawPixmap(iconRect, cancelPixmap);
        nameWidth += retryButtonSpace;
    }

    iconRect = option.rect;
    iconRect.translate(12+iconSize+nameWidth, 4);
    iconRect.setSize({iconSize, iconSize});
    auto mousePos = option.widget->mapFromGlobal(QCursor::pos());
    QPixmap cancelPixmap(":/remoteBrowser/task_cancel_small.png");
    if (iconRect.contains(mousePos, true)) {
        auto mask = cancelPixmap.createMaskFromColor(Qt::transparent);
        cancelPixmap.fill(Qt::red);
        cancelPixmap.setMask(mask);
        mActiveCancelRectGlobal.setTopLeft(option.widget->mapToGlobal(iconRect.topLeft()));
        mActiveCancelRectGlobal.setSize(iconRect.size());
        mActiveCancelIndex = index;
    }
    painter->drawPixmap(iconRect, cancelPixmap);
    painter->restore();
}

QSize TaskDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);
    int width = option.rect.width();
    if (width == 0)
        width = 300;
    auto size = QSize(width, sItemHeight);
    return size;
}

bool TaskDelegate::canCancel(const QModelIndex &index) const
{
    if (index != mActiveCancelIndex)
        return false;
    return (mActiveCancelRectGlobal.contains(QCursor::pos(), true));
}

bool TaskDelegate::canRetry(const QModelIndex &index) const
{
    if (index != mActiveCancelIndex)
        return false;
    return (mActiveRetryRectGlobal.contains(QCursor::pos(), true));
}

void TasksDialog::updateCountLabel(const QModelIndex &parent) {
    if (parent != mQueue->tasksIndex())
        return;
    int count = mQueue->rowCount(parent);
    ui->labelTaskCount->setText(QString::number(count));
}

void TasksDialog::onDataChanged(const QModelIndex &topLeft, const QModelIndex &botomRight, const QVector<int> roles) {
    if (topLeft != mQueue->currentTaskIndex() || botomRight != mQueue->currentTaskIndex())
        return;
    if (roles.size() == 1 && roles.first() == ScoutQueue::TitleRole) {
        QString name = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::TitleRole).toString();
        QString direction = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::DirectionRole).toString();
        if (direction == "upload")
            setTaskName(QString("uploading %1").arg(name));
        else
            setTaskName(QString("downloading %1").arg(name));
        return;
    }
    if (roles.size() == 2 && roles.contains(ScoutQueue::SizeRole) && roles.contains(ScoutQueue::ProgressRole)) {
        qint64 size = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::SizeRole).toLongLong();
        qint64 done = mQueue->data(mQueue->currentTaskIndex(), ScoutQueue::ProgressRole).toLongLong();
        if (size == 0) {
            ui->progressBar->setValue(0);
            ui->progressBar->setMaximum(0);
        }
        else {
            qint64 value = 1000*done/size;
            ui->progressBar->setValue(static_cast<int>(value));
            ui->progressBar->setMaximum(1000);
        }
        return;
    }
    refreshCurrentTask();
}

void TasksDialog::on_listView_clicked(const QModelIndex &index)
{
    if (mItemDelegate->canCancel(index))
        mQueue->cancelPendingTask(index.row());
    else if (mItemDelegate->canRetry(index))
        mQueue->retryTask(index.row());
}

void TasksDialog::on_buttonRetry_clicked()
{
    setErrorText("");
    mQueue->retryCurrentTask();
}

void TasksDialog::on_buttonCancel_clicked()
{
    mQueue->cancelCurrentTask();
}

void TasksDialog::setTaskName(const QString &taskName)
{
    mTaskName = taskName;
    refreshLabels();
}

void TasksDialog::setErrorText(const QString &errorText)
{
    mErrorText = errorText;
    refreshLabels();
}

void TasksDialog::refreshLabels()
{
    QFontMetrics fm(ui->labelTaskName->font());
    auto taskName = fm.elidedText(mTaskName, Qt::ElideMiddle, ui->labelTaskName->width());
    ui->labelTaskName->setText(taskName);
    auto errorText = fm.elidedText(mErrorText, Qt::ElideMiddle, ui->errorLabel->width());
    ui->errorLabel->setText(errorText);

}
