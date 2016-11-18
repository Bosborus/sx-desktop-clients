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

#include "synchistorymodel.h"
#include "util.h"

#include <QMimeDatabase>
#include <QPixmap>

SyncHistoryModel::SyncHistoryModel()
{
    SxDatabase::instance().getHistoryRowIds(mRowIds);
    connect(&SxDatabase::instance(), &SxDatabase::sig_historyChanged, this, &SyncHistoryModel::onHistoryChanged);
}

QModelIndex SyncHistoryModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (column == 0 && row < mRowIds.size())
        return createIndex(row, column);
    return QModelIndex();
}

QModelIndex SyncHistoryModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int SyncHistoryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return mRowIds.count();
}

int SyncHistoryModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QString SyncHistoryModel::timestampToFriendlyString(const QDateTime &now, const QDateTime &pastDate)
{
    if (now < pastDate)
    {
        return QString(tr("File from the future?"));
    }
    auto const delta = now.toTime_t() - pastDate.toTime_t();
    if (delta < 60) // less than 1 minute
    {
        return QString(tr("A moment ago"));
    }
    if (delta < 60*60) // less than 1 hour
    {
        return QString(tr("%n minute(s) ago", "", delta/60));
    }
    if (delta < 24*60*60) // less than 1 day
    {
        return QString(tr("%n hour(s) ago", "", delta/(60*60)));
    }
    if (delta < 7*24*60*60) // less than 7 days
    {
        return QString(tr("%n day(s) ago", "", delta/(24*60*60)));
    }
    if (delta < 31*24*60*60) // less than 30 days
    {
        return QString(tr("%n week(s) ago", "", delta/(7*24*60*60)));
    }
    if (delta < 365*24*60*60) // less than 1 year
    {
        return QString(tr("%n month(s) ago", "", delta/(30*24*60*60))); // approximate, but should be good enough
    }
    return QString(tr("%n year(s) ago", "", delta/(365*24*60*60)));
}

QVariant SyncHistoryModel::data(const QModelIndex &index, int role) const
{
    if ((role == Qt::DisplayRole || role == PathRole || role == EventDateRole || role == IconRole || role == ActionRole)
            && index.row() < mRowIds.size())
    {
        QString path;
        uint32_t eventDate;
        SxDatabase::ACTION eventType;
        qint64 rowId = mRowIds.value(index.row());
        if (SxDatabase::instance().getHistoryEntry(rowId, path, eventDate, eventType )) {
            if (role == EventDateRole)
            {
                return QVariant(timestampToFriendlyString(QDateTime::currentDateTimeUtc(), QDateTime::fromTime_t(eventDate)));
            }
            else if (role == PathRole)
            {
                return QVariant(path);
            }
            else if (role == ActionRole)
            {
                QString iconName;
                switch (eventType) {
                case SxDatabase::ACTION::UPLOAD:
                    iconName = "activity-upload";
                    break;
                case SxDatabase::ACTION::DOWNLOAD:
                    iconName = "activity-download";
                    break;
                case SxDatabase::ACTION::REMOVE_LOCAL:
                    iconName = "activity-removed-l";
                    break;
                case SxDatabase::ACTION::REMOVE_REMOTE:
                    iconName = "activity-removed-r";
                    break;
                case SxDatabase::ACTION::SKIP:
                    return QVariant();
                }
                if (isRetina())
                    iconName += "@2x";
                iconName += ".png";
                return QPixmap(":/mime/"+iconName);
            }
            else // IconRole
            {
                QMimeDatabase mimeDatabase;
                auto mimeType = mimeDatabase.mimeTypeForName(path);
                auto iconName = builtInIconForMime(mimeType.genericIconName());
                return QPixmap(":/mime/"+iconName);
            }
        }
    }
    return QVariant();
}

void SyncHistoryModel::onHistoryChanged(qint64 rowId, qint64 removeRowId)
{
    beginInsertRows(QModelIndex(), 0, 0);
    mRowIds.insert(0, rowId);
    endInsertRows();
    if (removeRowId != -1) {
        int index = mRowIds.indexOf(removeRowId);
        while (index < mRowIds.size()-1) {
            beginRemoveRows(QModelIndex(), index+1, index+1);
            mRowIds.removeAt(index+1);
            endRemoveRows();
        }
    }
}
