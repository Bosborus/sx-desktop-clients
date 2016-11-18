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

#include "trayiconcontroller.h"
#include <QHash>
#include <QPainter>
#include <QBitmap>
#include "maincontroller.h"
#include <QMenu>

static const QHash<int, QStringList> sBaseIcons = {
    {static_cast<int>(SxStatus::idle), {":/icons/tray@2x.png"}},
    {static_cast<int>(SxStatus::paused), {":/icons/tray-pause@2x.png"}},
    {static_cast<int>(SxStatus::inactive), {":/icons/tray-inactive@2x.png"}},
    {static_cast<int>(SxStatus::working), {":/icons/load-icon-1@2x.png",
                                           ":/icons/load-icon-2@2x.png",
                                           ":/icons/load-icon-3@2x.png",
                                           ":/icons/load-icon-4@2x.png"}}
};
static const QString warningIcon = ":/icons/tray-warning@2x.png";

TrayIconController::TrayIconController(QSystemTrayIcon *tray, ContextMenu *contextMenu, SxController *sxcontroller, QPair<QString, QString> shapeDesc, QObject *parent)
    : QObject (parent)
{
    mTray = tray;
    mContextMenu = contextMenu;
    mSxController = sxcontroller;
    mShapeDesc = shapeDesc;
    generateShape();
    mLastStatus = mSxController->status();
    mWarnings = mSxController->warningsCount() > 0;
    mTimer = new QTimer(this);
    mAnimationFrame = 0;
    mTray->setIcon(generateIcon());
    connect(mTimer, &QTimer::timeout, this, &TrayIconController::updateIcon);
    mTimer->start(200);
}

void TrayIconController::generateShape()
{
#ifdef NO_WHITELABEL
    QString backgrond;
    if (mShapeDesc.first.isEmpty()) {
        mBaseDir = ":/shapes/alt/";
        backgrond = ":/shapes/alt/background.png";
    }
    else {
        mBaseDir = ":/shapes/tray/";
        backgrond = ":/shapes/"+mShapeDesc.first;
    }
    QPixmap shape(backgrond);
    QPixmap coloredShape( shape.size() );
    QColor color(mShapeDesc.second);
    coloredShape.fill( color );
    coloredShape.setMask( shape.createMaskFromColor( Qt::transparent ) );
    mShape = coloredShape;
#else
    mShape = QPixmap();
#endif
}

QIcon TrayIconController::generateIcon()
{
    QStringList stateIcons = sBaseIcons.value(static_cast<int>(mLastStatus));
    QString iconPath = stateIcons.at(mAnimationFrame);
    if (!mShape.isNull())
        iconPath = mBaseDir + iconPath.mid(8);
    QPixmap pixmap(iconPath);
    QPixmap result(pixmap.size());
    result.fill(Qt::transparent);
    mAnimationFrame = (mAnimationFrame + 1) % stateIcons.length();
    QPainter painter(&result);
    QRectF target(0, 0, pixmap.width(), pixmap.height());
    if (!mShape.isNull()) {
        bool skip = mBaseDir ==":/shapes/alt/" && (mLastStatus == SxStatus::inactive || mLastStatus == SxStatus::paused);
        if (!skip) {
            QRectF source(0, 0, mShape.width(), mShape.height());
            painter.drawPixmap(target, mShape, source);
        }
    }
    painter.drawPixmap(target, pixmap, target);
    if (mWarnings) {
        QPixmap warningPixmap(warningIcon);
        QRectF source(0, 0, warningPixmap.width(), warningPixmap.height());
        painter.drawPixmap(target, warningPixmap, source);
    }
    return QIcon(result);
}

void TrayIconController::updateIcon()
{
    bool changed = false;
    if (mLastStatus != mSxController->status()) {
        mLastStatus = mSxController->status();
        changed = true;

        if (mLastStatus == SxStatus::inactive) {
            /*
            if (mWarnings) {
                if (mContextMenu->menu_status)
                    mContextMenu->menu_status->setText(tr("Server unreachable"));
                else
                    mContextMenu->statusMenu->setTitle(tr("Server unreachable"));
            }
            else {
                if (mContextMenu->menu_status)
                    mContextMenu->menu_status->setText(tr("Not configured"));
                else
                    mContextMenu->statusMenu->setTitle(tr("Not configured"));
            }
            */
            mContextMenu->menu_pauseResume->setText(QCoreApplication::translate("MainController", "Pause syncing"));
            mContextMenu->menu_pauseResume->setEnabled(false);
        }
        else if (mLastStatus == SxStatus::paused) {
            if (mContextMenu->menu_status)
                mContextMenu->menu_status->setText(QCoreApplication::translate("MainController", "Paused"));
            else
                mContextMenu->statusMenu->setTitle(QCoreApplication::translate("MainController", "Paused"));
            mContextMenu->menu_pauseResume->setText(QCoreApplication::translate("MainController", "Resume syncing"));
            mContextMenu->menu_pauseResume->setEnabled(true);
            mContextMenu->menu_pauseResume->setProperty("action_pause", false);
        }
        else {
            /*
            if (mLastStatus == SxStatus::idle) {
                if (mContextMenu->menu_status)
                    mContextMenu->menu_status->setText(tr("Up to date"));
                else
                    mContextMenu->statusMenu->setTitle(tr("Up to date"));
            }
            else {
                if (mContextMenu->menu_status)
                    mContextMenu->menu_status->setText(tr("Syncing..."));
                else
                    mContextMenu->statusMenu->setTitle(tr("Syncing..."));
            }
            */
            mContextMenu->menu_pauseResume->setText(QCoreApplication::translate("MainController", "Pause syncing"));
            mContextMenu->menu_pauseResume->setEnabled(true);
            mContextMenu->menu_pauseResume->setProperty("action_pause", true);
        }
    }
    if (mWarnings != (mSxController->warningsCount() > 0)) {
        mWarnings = mSxController->warningsCount() > 0;
        changed = true;
    }
    if (changed) {
        emit sig_stateChanged(mLastStatus);
        mAnimationFrame = 0;
        mTray->setIcon(generateIcon());
    }
    else {
        if (sBaseIcons.value(static_cast<int>(mLastStatus)).count() > 1) {
            mTray->setIcon(generateIcon());
        }
    }
}

void TrayIconController::updateShape(QPair<QString, QString> &shapeDesc)
{
    mShapeDesc = shapeDesc;
    generateShape();
    mTray->setIcon(generateIcon());
}
