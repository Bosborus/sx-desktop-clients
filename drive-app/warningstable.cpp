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

#include "warningstable.h"
#include <QHeaderView>
#include <QTimer>

WarningsTable::WarningsTable(const SxState *sxState, QWidget *parent) : QTableWidget(parent)
{
    mFirstPaint = true;
    mSxState = sxState;
    setColumnCount(2);
    setHorizontalHeaderItem(0, new QTableWidgetItem(tr("date")));
    setHorizontalHeaderItem(1, new QTableWidgetItem(tr("message")));
    horizontalHeader()->setStretchLastSection(true);
    QTimer::singleShot(0, this, SLOT(reloadWarnings()));
    connect(mSxState, &SxState::sig_appendRow, this, &WarningsTable::onAppendRow);
    connect(mSxState, &SxState::sig_removeRow, this, &WarningsTable::onRemoveRow);
    connect(mSxState, &SxState::sig_clear, this, &WarningsTable::onClear);
}

void WarningsTable::reloadWarnings()
{
    auto warnings = mSxState->warnings();
    setRowCount(warnings.count());
    for (int i=0; i<warnings.count(); i++) {
        const SxWarning &w = warnings.at(i);
        setItem(i, 0, new QTableWidgetItem(w.eventDate().toString("dd.MM.yyyy hh:mm:ss")));
        setItem(i, 1, new QTableWidgetItem(w.message()));
    }
    resizeColumnToContents(0);
}

void WarningsTable::onRemoveRow(int index)
{
    removeRow(index);
}

void WarningsTable::onAppendRow(const QDateTime &eventDate, const QString &message)
{
    int index = rowCount();
    insertRow(index);
    setItem(index, 0, new QTableWidgetItem(eventDate.toString("dd.MM.yyyy hh:mm:ss")));
    setItem(index, 1, new QTableWidgetItem(message));
    resizeColumnToContents(0);
    resizeRowToContents(index);
}

void WarningsTable::onClear()
{
    clearContents();
    setRowCount(0);
}

void WarningsTable::paintEvent(QPaintEvent *e)
{
    if (mFirstPaint) {
        mFirstPaint = false;
        QTimer::singleShot(0, this, SLOT(resizeRowsToContents()));
    }
    QTableWidget::paintEvent(e);
}
