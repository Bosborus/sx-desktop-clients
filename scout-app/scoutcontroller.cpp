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

#include <QApplication>
#include "scoutcontroller.h"
#include "logsmodel.h"
#include "scoutwizard.h"
#include "version.h"
#include "mainwindow.h"

ScoutController::ScoutController(QObject *parent) : QObject(parent)
{
    SxLog::instance().setLogModel(LogsModel::instance());
    SxLog::instance().setLogLevel(LogLevel::Debug);
    logInfo(QString("Starting SxScout %1").arg(SCOUTVERSION));
    if (!mConfig.isValid()) {
        ScoutWizard wizard(&mConfig);
        wizard.exec();
        if (!mConfig.isValid()) {
            QApplication::quit();
        }
    }

    mTaskDialog = nullptr;
    mQueue = new ScoutQueue(mConfig.clusterConfig());
    mQueueThread = new QThread();
    mQueueThread->start(QThread::NormalPriority);
    mQueue->moveToThread(mQueueThread);

    VersionCheck::initializeVersionCheck("SxScout", SCOUTVERSION,
                                         "http://cdn.skylable.com",
                                         "http://beta.s3.indian.skylable.com",
                                         "/check/sxscout-version?ver=%1&os=%2",
                                         "/sxscout/sxscout-%1",
                                         "sxscout_upgrade");
    VersionCheck::instance()->setEnabled(mConfig.checkVersion());
    VersionCheck::instance()->setCheckingBeta(mConfig.checkBetaVersion());

    openNewWindow();
}

void ScoutController::openNewWindow()
{
    MainWindow *mainWindow = new MainWindow(&mConfig, mQueue);
    mWindows.insert(mainWindow);
    connect(mainWindow, &MainWindow::windowClosed, this, &ScoutController::onWindowClosed);
    connect(mainWindow, &MainWindow::showTaskDialog, this, &ScoutController::showTaskDialog);
    connect(mainWindow, &MainWindow::showHideTasksDialogClicked, this, &ScoutController::showHideTaskDialog);
    connect(mainWindow, &MainWindow::clusterConfigChanged, this, &ScoutController::onClusterConfigChanged);
    mainWindow->show();
    mainWindow->raise();
    mainWindow->activateWindow();
}

void ScoutController::messageReceived(const QString &message)
{
    if (message == "newWindow")
        openNewWindow();
    else if (message == "closeAll")
        QApplication::quit();
}

void ScoutController::showTaskDialog()
{
    if (mTaskDialog == nullptr) {
        mTaskDialog = new TasksDialog(mQueue);
        connect(mTaskDialog, &QDialog::finished, [this]() {
            mTaskDialog->deleteLater();
            mTaskDialog = nullptr;
        });
    }
    mTaskDialog->show();
}

void ScoutController::showHideTaskDialog()
{
    if (mTaskDialog == nullptr || mTaskDialog->isHidden())
        showTaskDialog();
    else
        mTaskDialog->close();
}

void ScoutController::onClusterConfigChanged()
{
    if (mTaskDialog != nullptr)
        mTaskDialog->close();
    delete mQueue;
    mQueue = new ScoutQueue(mConfig.clusterConfig());
    mQueue->moveToThread(mQueueThread);
    foreach (auto window, mWindows) {
        window->reloadConfig(mQueue);
    }
}


void ScoutController::onWindowClosed(MainWindow* window)
{
    mWindows.remove(window);
    if (mWindows.isEmpty())
        QApplication::quit();
}
