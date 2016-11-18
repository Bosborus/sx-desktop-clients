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

#include "sxstate.h"

SxWarning::SxWarning(const QString &volume, const QString &file, const QString &message, bool critical)
{
    mVolume = volume;
    mFile = file;
    mMessage = message;
    mCritical = critical;
    mEventDate = QDateTime::currentDateTime();
}

bool SxWarning::isEqual(const QString &volume, const QString &file) const
{
    return mVolume == volume && mFile == file;
}

QDateTime SxWarning::eventDate() const
{
    return mEventDate;
}

QString SxWarning::message() const
{
    return mMessage;
}

SxState::SxState(QObject *parent) : QObject(parent)
{
    mStatus = SxStatus::inactive;
}

SxStatus SxState::status() const
{
    return mStatus;
}

int SxState::warningsCount() const
{
    return mWarnings.count();
}

const QList<SxWarning> &SxState::warnings() const
{
    return mWarnings;
}

QString SxState::toString() const
{
    if (!mWarnings.isEmpty() && (mStatus == SxStatus::idle || mStatus == SxStatus::working))
        return "warning";
    switch (mStatus) {
    case SxStatus::idle:
        return "idle";
    case SxStatus::working:
        return "working";
    case SxStatus::paused:
        return "paused";
    case SxStatus::inactive:
        return "inactive";
    }
    return "error";
}

void SxState::addWarning(const QString &volume, const QString &file, const QString &message, bool critical)
{
    if (mWarningsSet.contains(volume+file)) {
        if (mWarningsSet.value(volume+file)==message)
            return;
        removeWarning(volume, file);
    }
    mWarnings.append(SxWarning(volume, file, message, critical));
    mWarningsSet.insert(volume+file, message);
    emit sig_appendRow(mWarnings.last().eventDate(), mWarnings.last().message());
}

void SxState::removeWarning(const QString &volume, const QString &file)
{
    mWarningsSet.remove(volume+file);
    for (int i=0; i<mWarnings.count(); i++) {
        if (mWarnings.at(i).isEqual(volume, file)) {
            mWarnings.removeAt(i);
            emit sig_removeRow(i);
            return;
        }
    }
}

void SxState::setStatus(SxStatus status)
{
    mStatus = status;
}

void SxState::clearWarnings()
{
    mWarnings.clear();
    mWarningsSet.clear();
    emit sig_clear();
}
