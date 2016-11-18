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

#ifndef MainWindow_H
#define MainWindow_H

#include <QMainWindow>
#include "scoutconfig.h"
#include "scoutmodel.h"
#include "scoutqueue.h"
#include "tasksdialog.h"
#include <QItemDelegate>
#include <QHash>

namespace Ui {
class MainWindow;
}

class VolumeViewDelegate : public QItemDelegate {
    Q_OBJECT
public:
    VolumeViewDelegate(ScoutModel *model, QObject *parent=nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
private:
    ScoutModel *mModel;
    const int mMargin = 4;
    const int mIconSize = 32;
    const int mProgressHeight = 4;
    const int mItemHeight = mIconSize+2*mMargin;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(ScoutConfig *config, ScoutQueue *queue, QWidget *parent = 0);
    ~MainWindow();
    static QPixmap iconForMimeType(QString mimeType, bool smallIcon);
    void reloadConfig(ScoutQueue *queue);

public slots:
    void showError(const QString& errorMessage);

signals:
    void clusterConfigChanged();
    void windowClosed(MainWindow* window);
    void showTaskDialog();
    void showHideTasksDialogClicked();

private:
    bool unlockVolume(const QString& volume);

private slots:
    void refreshActionsAndTitle();
    void recalculateColumnCount();
    void onVolumeClicked(const QModelIndex& index);
    void openIndex(const QModelIndex& index);
    void onNewVersionAvailable(const QString &version);
    void queueStarted();
    void queueFinished();
    void clusterInitialized();
    void onAction(const QString &actionName);
    void showTasksWarning(bool showWarning);
    void on_actionHome_triggered();
    void on_actionUp_triggered();
    void on_actionNext_triggered();
    void on_actionBack_triggered();
    void on_actionTasks_triggered();
    void on_actionUpload_triggered();
    void on_actionRefresh_triggered();
    void on_actionSettings_triggered();
    void on_buttonCloseErrorArea_clicked();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *) override;

private:
    Ui::MainWindow *ui;
    ScoutConfig *mConfig;
    ScoutModel *mModel;
    ScoutQueue *mQueue;
    QDateTime mLastDirDoubleClickedTime;
};

#endif // MainWindow_H
