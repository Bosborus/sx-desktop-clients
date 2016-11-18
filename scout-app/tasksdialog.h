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

#ifndef TasksDialog_H
#define TasksDialog_H

#include <QDialog>
#include <QAbstractItemDelegate>
#include "scoutqueue.h"

namespace Ui {
class TasksDialog;
}

class TaskDelegate : public QAbstractItemDelegate {
    Q_OBJECT
public:
    explicit TaskDelegate(ScoutQueue *model, QObject* parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool canCancel(const QModelIndex &index) const;
    bool canRetry(const QModelIndex &index) const;

private:
    const int sItemHeight = 40;
    ScoutQueue *mQueue;
    mutable QRect mActiveCancelRectGlobal;
    mutable QRect mActiveRetryRectGlobal;
    mutable QModelIndex mActiveCancelIndex;
};

class TasksDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TasksDialog(ScoutQueue *queue, QWidget *parent = nullptr);
    ~TasksDialog();
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void setTaskName(const QString &taskName);
    void setErrorText(const QString &errorText);
    void refreshLabels();
private slots:
    void on_toolButton_clicked();
    void refreshCurrentTask();
    void updateCountLabel(const QModelIndex& parent);
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &botomRight, const QVector<int> roles);
    void on_listView_clicked(const QModelIndex &index);
    void on_buttonRetry_clicked();
    void on_buttonCancel_clicked();
private:
    Ui::TasksDialog *ui;
    ScoutQueue *mQueue;
    TaskDelegate *mItemDelegate;
    QString mTaskName;
    QString mErrorText;
};

#endif // TasksDialog_H
