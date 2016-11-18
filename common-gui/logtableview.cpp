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

#include "logsmodel.h"
#include "logtableview.h"
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QModelIndexList>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QScrollBar>

LogTableView::LogTableView(QWidget *parent)
    :QTableView(parent)
{
    m_menu = new QMenu(this);
    m_actionCopy = m_menu->addAction(tr("copy"));
    connect(m_actionCopy, &QAction::triggered, this, &LogTableView::copyToClipboard);
    //connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &LogTableView::onVerticalScrollBarValueChanged);
}

LogTableView::~LogTableView()
{
}

void LogTableView::keyPressEvent(QKeyEvent *event)
{
    if (event->key()== Qt::Key_C && (event->modifiers() & Qt::ControlModifier))
    {
        copyToClipboard();
    }
}

void LogTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint p = mapToGlobal(event->pos());
    m_actionCopy->setEnabled(!selectedIndexes().isEmpty());
    m_menu->popup(p, m_actionCopy);
}

void LogTableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    QModelIndex a = indexAt(QPoint(1,1));
    QModelIndex b = indexAt(QPoint(1,height()-1));
    resizeRows(a.row()-2, b.row());
}

int LogTableView::sizeHintForRow(int row) const
{
    Q_UNUSED(row);
    //------------------------------------------------------------------------
    // TODO: investigate why QTableView::sizeHintForRow(row) sometimes hang //
    //------------------------------------------------------------------------
    //int sizeHint = QTableView::sizeHintForRow(row);
    int sizeHint = 23;
    return sizeHint+7;
}

void LogTableView::resizeRows(int first_row, int last_row)
{
    if (model() == nullptr)
        return;
    if (first_row<0)
        first_row=0;
    if (last_row == -1)
        last_row = model()->rowCount(QModelIndex());
    for (int i=first_row; i<=last_row; i++)
        resizeRowToContents(i);
}

void LogTableView::copyToClipboard()
{
    QModelIndexList cells = selectedIndexes();
    foreach (const QModelIndex &cell, cells) {
        if (cell.column())
            cells.removeOne(cell);
    }
    auto *logModel = qobject_cast<LogsModel*>(model());
    QString output;
    foreach (const QModelIndex &cell, cells) {
        int row = cell.row();
        output += logModel->line(row)+"\n";
    }
    QApplication::clipboard()->setText(output);

}

void LogTableView::onVerticalScrollBarValueChanged(int value)
{
    QModelIndex b = indexAt(QPoint(1,height()-1));
    resizeRows(value-2, b.row());
}

