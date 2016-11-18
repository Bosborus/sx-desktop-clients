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

#include "synchistoryitemdelegate.h"
#include <QPainter>
#include <QApplication>
#include "util.h"
#include "synchistorymodel.h"

SyncHistoryItemDelegate::SyncHistoryItemDelegate(QString &vcluster, QObject *parent)
    : QItemDelegate(parent), m_vcluster(vcluster)
{
}

void SyncHistoryItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto history = qobject_cast<const SyncHistoryModel*>(index.model());
    Q_ASSERT(history != nullptr);

    painter->save();
    painter->setFont(option.font);
    painter->setPen(Qt::black);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    auto const path = hideVcluster(history->data(index, SyncHistoryModel::PathRole).toString());
    auto const eventdate = history->data(index, SyncHistoryModel::EventDateRole).toString();
    auto const icon = history->data(index, SyncHistoryModel::IconRole).value<QPixmap>();
    auto const activityIcon = history->data(index, SyncHistoryModel::ActionRole).value<QPixmap>();

    // draw activityIcon
    auto activityRect = option.rect;
    activityRect.translate(4, 2);
    activityRect.setWidth(36);
    activityRect.setHeight(36);
    painter->drawPixmap(activityRect, activityIcon);

    // draw icon
    auto iconRect = option.rect;
    iconRect.translate(8 + activityRect.right(), 4);
    iconRect.setWidth(36);
    iconRect.setHeight(32);
    painter->drawPixmap(iconRect, icon);

    // draw file name
    auto rect = option.rect;
    rect.translate(8 + iconRect.right(), 0.0f);
    rect.setHeight(static_cast<int>(rect.height() / 2.0f));
    painter->drawText(rect, 0, path);

    // draw file sync date
    painter->setPen(Qt::gray);
    rect.translate(0.0f, rect.height());
    painter->drawText(rect, 0, eventdate);

    // draw horizontal line at the bottom
    QPen pen(Qt::gray);
    pen.setWidthF(0.5f);
    painter->setPen(pen);
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

    painter->restore();
}

QSize SyncHistoryItemDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex& /*index*/) const
{
    QFontMetrics fm(option.font);
    return QSize(200, static_cast<int>(fm.height() * 2.8f));
}

QString SyncHistoryItemDelegate::hideVcluster(const QString &path) const
{
    if (!path.startsWith(m_vcluster+"."))
        return path;
    return path.mid(m_vcluster.length()+1);
}

