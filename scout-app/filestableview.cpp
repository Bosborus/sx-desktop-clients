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

#include "filestableview.h"
#include "sxlog.h"
#include <QApplication>
#include <QDrag>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QInputDialog>
#include <QClipboard>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include "sharefiledialog.h"
#include "detailsdialog.h"
#include "progressdialog.h"
#include "scoutmimedata.h"
#include "mainwindow.h"
#include "sxcluster.h"
#include "sxfilter.h"

FileViewDelegate::FileViewDelegate(ScoutModel *model, QTableView *view, QObject *parent) : QItemDelegate (parent)
{
    mModel = model;
    mView = view;
}

void FileViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    static const QColor sSelectionColor = "#e0e0e0";
    static const QColor sSelectedTextColor = "#0069d9";

    QString mimeType = mModel->data(index, ScoutModel::MimeTypeRole).toString();
    if (mimeType.isEmpty())
        return;

    QString name = mModel->data(index, ScoutModel::NameRole).toString();
    QPixmap mimeIcon = MainWindow::iconForMimeType(mimeType, false);

    painter->save();
    painter->setFont(option.font);
    painter->setPen(Qt::black);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

    auto iconRect = option.rect;
    int iconX = (iconRect.width() - sIconWidth)/2;
    iconRect.translate(iconX, sMargin);
    iconRect.setWidth(sIconWidth);
    iconRect.setHeight(sIconHeight);

    QFontMetrics fm(option.font);
    auto rect = fm.boundingRect(0,0, 150-2*sMargin-2*sTextPadding, 0,
                                Qt::AlignHCenter | Qt::TextWrapAnywhere,
                                index.data(ScoutModel::NameRole).toString());
    int expectedHeight = 3*sMargin+sIconHeight+rect.height();
    int nameRectWidth = rect.width()+2*sTextPadding;
    int nameRectHeight = rect.height()+2*sTextPadding;
    rect = option.rect;
    int nameRectMargin = (option.rect.width()-nameRectWidth)/2;
    rect.translate(nameRectMargin, sMargin + sIconHeight+2);
    rect.setSize({nameRectWidth, nameRectHeight});

    if (expectedHeight > option.rect.height())
        emit setRowHeight(index.row(), expectedHeight);

    if(option.state & QStyle::State_Enabled)
        painter->fillRect(option.rect, Qt::white);

    auto mousePos = option.widget->mapFromGlobal(QCursor::pos());
    iconRect.setHeight(iconRect.height()+2);
    bool highlight = iconRect.contains(mousePos) || rect.contains(mousePos);
    iconRect.setHeight(iconRect.height()-2);
    bool drawDragSelection = highlight && mDragTargetIndex == index;

    if (drawDragSelection || (option.showDecorationSelected && (option.state & QStyle::State_Selected))) {
        painter->setBrush(QBrush(sSelectionColor));
        painter->setPen(sSelectionColor);
        painter->drawRoundedRect(iconRect, 2, 2);
        painter->setBrush(QBrush(sSelectedTextColor));
        painter->setPen(sSelectedTextColor);
        painter->drawRoundedRect(rect, 2, 2);
        painter->setPen(Qt::white);
    }
    painter->drawPixmap(iconRect, mimeIcon);
    painter->drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, name, &rect);

    if (highlight) {
        if (!mDragTargetIndex.isValid())
            painter->fillRect(iconRect, QColor(255,255,255,128));
        mPointedIndex = index;
    }
    else if (mPointedIndex == index) {
        mPointedIndex = QModelIndex();
    }

    painter->restore();
}

QSize FileViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)
    QFontMetrics fm(option.font);
    auto rect = fm.boundingRect(0,0, 150-2*sMargin-2*sTextPadding, 0,
                                Qt::AlignHCenter | Qt::TextWrapAnywhere,
                                index.data(ScoutModel::NameRole).toString());
    return QSize(150, 3*sMargin+sIconHeight+rect.height()+2*sTextPadding);
}

QModelIndex FileViewDelegate::lastPointedIndex() const
{
    return mPointedIndex;
}

QModelIndex FileViewDelegate::dragTargetIndex() const
{
    return mDragTargetIndex;
}

bool FileViewDelegate::isIndexActive(const QModelIndex &index) const
{
    if (!index.isValid())
        return false;
    return index == mPointedIndex;
}

bool FileViewDelegate::canSelect(const QModelIndex &index, const QRect &itemRect, const QRect &selectionRect)
{
    if (!index.isValid())
        return false;
    auto iconRect = itemRect;
    int iconX = (iconRect.width() - sIconWidth)/2;
    iconRect.translate(iconX, sMargin);
    iconRect.setWidth(sIconWidth);
    iconRect.setHeight(sIconHeight);
    if (iconRect.intersects(selectionRect))
        return true;
    return false;
}

void FileViewDelegate::setDragTarget(const QModelIndex &index)
{
    mDragTargetIndex = index;
}

FilesTableView::FilesTableView(QWidget *parent) : QTableView(parent)
{
    mModel = nullptr;
    mDrag = nullptr;
    mDragStart = nullptr;
    mConfig = nullptr;
    mMenu = nullptr;
    mSelectionFrame = nullptr;
    mScrollSpeed = 0;
    mClearSelectionAfterRelease = false;
    mPasteInfo = nullptr;
    setAcceptDrops(true);
    setMouseTracking(true);
    mCloseMenuTime = QDateTime::currentDateTime();
    connect(&mScrollTimer, &QTimer::timeout, this, &FilesTableView::scroll);
}

void FilesTableView::mousePressEvent(QMouseEvent *event)
{
    mClearSelectionAfterRelease = false;
    if (mModel == nullptr)
        return;
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;
    if (mCloseMenuTime.msecsTo(QDateTime::currentDateTime()) < 10)
        return;

    auto index = indexAt(event->pos());
    auto mimeType = mModel->data(index, ScoutModel::MimeTypeRole).toString();

    int createSelector = false;
    int showDirectoryContextMenu = false;
    auto keyboardModifiers = QApplication::keyboardModifiers();

    if (mimeType.isEmpty()) {
        if (event->button() == Qt::RightButton)
            showDirectoryContextMenu = true;
        else
            createSelector = true;
    }
    else {
        auto itemDelegate = qobject_cast<FileViewDelegate*>(this->itemDelegate());
        if (itemDelegate == nullptr || !itemDelegate->isIndexActive(index)) {
            if (event->button() == Qt::RightButton)
                showDirectoryContextMenu = true;
            else
                createSelector = true;
        }
    }
    if (showDirectoryContextMenu) {
        clearSelection();
        if (mModel->currentVolume().isEmpty())
            return;
        mMenu = new QMenu(this);
        QAction *action;
        mMenu->addAction(tr("Create directory"), this, SLOT(actionCreateDirectory()));
        mMenu->addAction(tr("Add file"), this, SLOT(actionUpload()));
        mMenu->addSeparator();
        action = mMenu->addAction(tr("Paste"), this, SLOT(actionPaste()));
        action->setEnabled(canPaste());
        mMenu->addSeparator();
        mMenu->addAction(tr("Details"), this, SLOT(actionDetails()));
        mMenu->popup(event->globalPos());
        connect(mMenu, &QMenu::aboutToHide, this, &FilesTableView::onCloseContextMenu);
        return;
    }
    if (createSelector) {
        if (keyboardModifiers.testFlag(Qt::ControlModifier))
            mPreviousSelected = selectionModel()->selection();
        else
            clearSelection();
        QPoint start = mapFromGlobal(QCursor::pos());
        mSelectionStart.setX(start.x()+horizontalScrollBar()->value());
        mSelectionStart.setY(start.y()+verticalScrollBar()->value());
        mSelectionFrame = new QRubberBand(QRubberBand::Rectangle, this);
        mSelectionFrame->setGeometry(mSelectionStart.x(), mSelectionStart.y(), 1, 1);
        mSelectionFrame->show();
        return;
    }

    QItemSelectionModel::SelectionFlag selectionFlag;
    if (keyboardModifiers.testFlag(Qt::ShiftModifier)) {
        if (!mCurrentIndex.isValid()) {
            mSelectionStartIndex = mCurrentIndex = index;
            selectionModel()->select(index, QItemSelectionModel::Select);
            return;
        }
        selectRange(mSelectionStartIndex, index);
        mCurrentIndex = index;
        return;
    }
    mSelectionStartIndex = index;
    if (keyboardModifiers.testFlag(Qt::ControlModifier)) {
        if (event->button() == Qt::RightButton)
            selectionFlag = QItemSelectionModel::Select;
        else
            selectionFlag = QItemSelectionModel::Toggle;
    }
    else {
        if (event->button() == Qt::RightButton && selectionModel()->selectedIndexes().contains(index))
            selectionFlag = QItemSelectionModel::Select;
        else {
            if (selectionModel()->selectedIndexes().contains(index))
                mClearSelectionAfterRelease = true;
            else
                clearSelection();
            selectionFlag = QItemSelectionModel::Select;
        }
    }
    selectionModel()->select(index, selectionFlag);
    mCurrentIndex = index;
    if (event->button() == Qt::RightButton) {
        if (mimeType.startsWith("Volume"))
            return;
        mMenu = new QMenu(this);
        QAction *action;
        if (mimeType == "Directory") {
            action = mMenu->addAction(tr("Open"), this, SLOT(actionOpen()));
            if (selectionModel()->selection().count()!=1)
                action->setEnabled(false);
        }
        mMenu->addAction(tr("Download"), this, SLOT(actionDownload()));
        action = mMenu->addAction(tr("Public link"), this, SLOT(actionPublicLink()));
        if (selectionModel()->selection().count()!=1 || mConfig == 0 || mModel->isAesVolume(mModel->currentVolume())) {
            action->setEnabled(false);
        }
        else if (mimeType == "Directory") {
            if (mModel->sxshareAddress().isEmpty())
                action->setEnabled(false);
        }
        else {
            if (!mModel->sharingEnabled())
                action->setEnabled(false);
        }
        mMenu->addSeparator();
        mMenu->addAction(tr("Cut"), this, SLOT(actionCut()));
        mMenu->addAction(tr("Copy"), this, SLOT(actionCopy()));
        action = mMenu->addAction(tr("Rename"), this, SLOT(actionRename()));
        if (selectionModel()->selection().count()!=1)
            action->setEnabled(false);
        mMenu->addAction(tr("Remove"), this, SLOT(actionRemove()));
        mMenu->addSeparator();
        mMenu->addAction(tr("Details"), this, SLOT(actionDetails()));
        mMenu->popup(event->globalPos());
        connect(mMenu, &QMenu::aboutToHide, this, &FilesTableView::onCloseContextMenu);
    }
    else {
        if (mimeType.startsWith("Volume"))
            return;
        mDragStart = new QPoint(event->pos());
    }
}

void FilesTableView::mouseReleaseEvent(QMouseEvent *)
{
    mScrollTimer.stop();
    mScrollSpeed = 0;

    if (mSelectionFrame != nullptr) {
        mPreviousSelected.clear();
        mSelectionFrame->deleteLater();
        mSelectionFrame= nullptr;
        repaint();
        if (!selectionModel()->selectedIndexes().isEmpty())
            mCurrentIndex = selectionModel()->selectedIndexes().last();
    }
    if (mDragStart != nullptr) {
        delete mDragStart;
        mDragStart = nullptr;
    }
    if (mClearSelectionAfterRelease) {
        mClearSelectionAfterRelease = false;
        auto index = mCurrentIndex;
        clearSelection();
        selectionModel()->select(index, QItemSelectionModel::Select);
    }
}

void FilesTableView::mouseMoveEvent(QMouseEvent *event)
{
    if (mDrag != nullptr)
        return;

    if (mSelectionFrame != nullptr) {
        if (event->y() < 0) {
            int mult = event->y()/(-10)+1;
            mScrollSpeed = - mult*verticalScrollBar()->singleStep();
            if (!mScrollTimer.isActive())
                mScrollTimer.start(10);
        }
        else if (event->y() >= viewport()->height()) {
            int mult = (event->y()-viewport()->height())/10+1;
            mScrollSpeed = mult*verticalScrollBar()->singleStep();
            if (!mScrollTimer.isActive())
                mScrollTimer.start(10);
        }
        else {
            mScrollTimer.stop();
            mScrollSpeed = 0;
        }

        auto pos = event->pos();
        QPoint start;
        start.setX(mSelectionStart.x()-horizontalScrollBar()->value());
        start.setY(mSelectionStart.y()-verticalScrollBar()->value());
        auto rect =  QRect(start, pos).normalized();
        mSelectionFrame->setGeometry(rect);
        selectionModel()->clear();
        selectionModel()->select(mPreviousSelected, QItemSelectionModel::Select);

        int firstRow = rowAt(rect.top());
        int lastRow = rowAt(rect.bottom());
        int firstColumn = columnAt(rect.left());
        int lastColumn = columnAt(rect.right());

        int rowCount = mModel->rowCount(mModel->filesIndex());
        int columnCount = mModel->columnCount(mModel->filesIndex());;
        int filesCount = mModel->filesCount();

        if (firstRow == -1 && rect.y() >= 0)
            goto afterSelection;
        if (firstColumn == -1 && rect.x() >= 0)
            goto afterSelection;

        if (firstRow < 0)
            firstRow = 0;
        if (firstColumn < 0)
            firstColumn = 0;
        if (lastRow == -1)
            lastRow = rowCount-1;
        if (lastColumn == -1)
            lastColumn = columnCount-1;

        QModelIndex topLeft = mModel->index(firstRow, firstColumn, mModel->filesIndex());

        if (!FileViewDelegate::canSelect(topLeft, visualRect(topLeft), rect)) {
            if (firstRow == lastRow && firstColumn == lastColumn)
                goto afterSelection;
            topLeft = mModel->index(firstRow+1, firstColumn, mModel->filesIndex());
            if (!FileViewDelegate::canSelect(topLeft, visualRect(topLeft), rect)) {
                topLeft = mModel->index(firstRow, firstColumn+1, mModel->filesIndex());
                if (!FileViewDelegate::canSelect(topLeft, visualRect(topLeft), rect))
                    topLeft = mModel->index(firstRow+1, firstColumn+1, mModel->filesIndex());
            }
        }

        QModelIndex bottomRight = mModel->index(lastRow, lastColumn, mModel->filesIndex());
        if (!FileViewDelegate::canSelect(bottomRight, visualRect(bottomRight), rect)) {
            bottomRight = mModel->index(lastRow-1, lastColumn, mModel->filesIndex());
            if (!FileViewDelegate::canSelect(bottomRight, visualRect(bottomRight), rect)) {
                bottomRight = mModel->index(lastRow, lastColumn-1, mModel->filesIndex());
                if (!FileViewDelegate::canSelect(bottomRight, visualRect(bottomRight), rect)) {
                    bottomRight = mModel->index(lastRow-1, lastColumn-1, mModel->filesIndex());
                    if (!FileViewDelegate::canSelect(bottomRight, visualRect(bottomRight), rect))
                        goto afterSelection;
                }
            }
        }

        if (bottomRight.row() == rowCount-1) {
            if (rowCount*columnCount>filesCount) {
                int lastRow = columnCount -rowCount*columnCount+filesCount-1;
                if (topLeft.column()>lastRow) {
                    if (topLeft.row() == bottomRight.row())
                        goto afterSelection;
                    bottomRight = mModel->index(bottomRight.row()-1, bottomRight.column(), mModel->filesIndex());
                }
                else {
                if (topLeft.row() == bottomRight.row()) {
                    bottomRight = mModel->index(bottomRight.row(), lastRow, mModel->filesIndex());
                }
                else {
                    auto left = mModel->index(bottomRight.row(), topLeft.column(), mModel->filesIndex());
                    auto right = mModel->index(bottomRight.row(), lastRow, mModel->filesIndex());
                    QItemSelection selection(left, right);
                    selectionModel()->select(selection, QItemSelectionModel::Toggle);
                    bottomRight = mModel->index(bottomRight.row()-1, bottomRight.column(), mModel->filesIndex());
                }
                }
            }
        }
        QItemSelection selection(topLeft, bottomRight);
        selectionModel()->select(selection, QItemSelectionModel::Toggle);

        /*
        for (int r = firstRow+1; r<r1; r++) {
            for (int c=firstColumn+1; c<c1; c++) {
                auto index = mModel->index(r, c, mModel->filesIndex());
                if (r == rowCount-1 && mModel->data(index, ScoutModel::MimeTypeRole).toString().isEmpty())
                    break;
                selectionModel()->select(index, QItemSelectionModel::Toggle);
            }
        }
        QList<QModelIndex> testList;
        if (firstRow >= 0) {
            for (int c=firstColumn==-1 ? 0 : firstColumn ; c<=c1; c++) {
                testList.append(mModel->index(firstRow, c, mModel->filesIndex()));
            }
        }
        if (lastRow >= 0 && firstRow != lastRow) {
            for (int c=firstColumn==-1 ? 0 : firstColumn ; c<=c1; c++) {
                testList.append(mModel->index(lastRow, c, mModel->filesIndex()));
            }
        }
        if (firstColumn >= 0) {
            for (int r = firstRow==-1 ? 1 : firstRow+1; r<r1; r++) {
                testList.append(mModel->index(r, firstColumn, mModel->filesIndex()));
            }
        }
        if (lastColumn >= 0 && firstColumn != lastColumn) {
            for (int r = firstRow==-1 ? 1 : firstRow+1; r<r1; r++) {
                testList.append(mModel->index(r, lastColumn, mModel->filesIndex()));
            }
        }
        foreach (auto index, testList) {
            if (index.row() == rowCount-1 && mModel->data(index, ScoutModel::MimeTypeRole).toString().isEmpty())
                continue;
            if (FileViewDelegate::canSelect(index, visualRect(index), font(), rect))
                selectionModel()->select(index, QItemSelectionModel::Toggle);
        }
        */
    }
    afterSelection:

    if (mDrag == nullptr && mDragStart != nullptr) {
        int w = event->pos().x() - mDragStart->x();
        int h = event->pos().y() - mDragStart->y();
        int lengthSquared = w*w+h*h;
        if (lengthSquared >= 400) {
            delete mDragStart;
            mDragStart = nullptr;
            mDrag = new QDrag(this);
            QMimeData *mimeData = selectionToMimeData();
            mimeData->setData("sxscout/operationType", "copy");

            QPixmap pixmap(64, 64);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);

            int count = selectionModel()->selectedIndexes().count();
            if (count==1) {
                painter.drawPixmap(pixmap.rect(), QPixmap(":/remoteBrowser/drag_single.png"));
            }
            else {
                painter.drawPixmap(pixmap.rect(), QPixmap(":/remoteBrowser/drag_multiple.png"));
                QString text;
                if (count <= 10000)
                    text = QString::number(count);
                else
                    text = "9999+";
                QFontMetrics fm(painter.font());
                QRect textRect = fm.boundingRect(text);
                int w = textRect.width()+4;
                int h = textRect.height()+4;
                if(w < h)
                    w = h;
                textRect.setRect(0, pixmap.height() - h, w, h);
                painter.setPen(Qt::red);
                painter.setBrush(QBrush(Qt::red));
                painter.drawRoundedRect(textRect, 2, 2);
                painter.setPen(Qt::white);
                painter.drawText(textRect, Qt::AlignCenter, text);
            }

            mDrag->setMimeData(mimeData);
            mDrag->setPixmap(pixmap);
            mDrag->exec(Qt::CopyAction);
            #ifdef Q_OS_MAC
            auto dropDestination = mDrag->mimeData()->data("dropDestination");
            QUrl url;
            QDir dir;
            if (dropDestination.isEmpty())
                goto cleanup;
            url = QUrl::fromEncoded(dropDestination);
            if (!url.isLocalFile())
                goto cleanup;
            dir = QDir(url.path());
            if (dir.exists()) {
                ScoutMimeData *mime = qobject_cast<ScoutMimeData*>(mimeData);
                if (mime!=nullptr) {
                    mime->requestDownloadTo(dir.absolutePath());
                }
            }
            cleanup:
            #endif
            mDrag->deleteLater();
            mDrag = nullptr;
        }
    }

    auto pos = mapFromGlobal(QCursor::pos());
    if (!rect().contains(pos))
        return;
    auto index = indexAt(pos);

    auto itemDelegate = qobject_cast<FileViewDelegate*>(this->itemDelegate());
    if (itemDelegate != nullptr) {
        auto lastIndex = itemDelegate->lastPointedIndex();
        if (lastIndex.isValid()) {
            if (lastIndex != index)
                update(lastIndex);
            else
                return;
        }
    }
    if (!index.isValid())
        return;
    update(index);
}

void FilesTableView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (mModel == nullptr)
        return;

    auto index = indexAt(event->pos());
    auto mimeType = mModel->data(index, ScoutModel::MimeTypeRole).toString();
    if (mimeType.isEmpty())
        return;
    auto itemDelegate = qobject_cast<FileViewDelegate*>(this->itemDelegate());
    if (itemDelegate == nullptr || !itemDelegate->isIndexActive(index))
        return;
    emit openIndex(index);
    mCurrentIndex = QModelIndex();
}

void FilesTableView::keyPressEvent(QKeyEvent *event)
{
    QList<int> arrowKeys = {Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down};
    if (mModel == nullptr)
        return;
    int key = event->key();
    if (key == Qt::Key_Enter || key == Qt::Key_Return) {
        if (selectionModel()->selection().count() != 1 || !mCurrentIndex.isValid())
            return;
        emit openIndex(mCurrentIndex);
        mCurrentIndex = QModelIndex();
        return;
    }
    if (key == Qt::Key_Backspace) {
        emit signalAction("up");
        return;
    }
    if (key == Qt::Key_F5) {
        emit signalAction("refresh");
        return;
    }
    if (key == Qt::Key_Delete) {
        if (!mModel->currentVolume().isEmpty() && !selectionModel()->selectedIndexes().isEmpty()) {
            actionRemove();
        }
        return;
    }
    if (key == Qt::Key_A && event->modifiers().testFlag(Qt::ControlModifier) && !mModel->currentVolume().isEmpty()) {

        int columnsCount = mModel->columnCount(mModel->filesIndex());
        int rowsCount = mModel->rowCount(mModel->filesIndex());
        int filesCount = mModel->filesCount();
        int lastRowCount;

        if (columnsCount*rowsCount>filesCount) {
            lastRowCount = filesCount-columnsCount*(rowsCount-1);
            --rowsCount;
        }
        else
            lastRowCount = 0;
        QItemSelection itemSelection;
        if (rowsCount > 0)
            itemSelection.select(mModel->index(0,0,mModel->filesIndex()), mModel->index(rowsCount-1,columnsCount-1,mModel->filesIndex()));
        if (lastRowCount > 0)
            itemSelection.select(mModel->index(rowsCount,0,mModel->filesIndex()), mModel->index(rowsCount,lastRowCount-1,mModel->filesIndex()));
        selectionModel()->select(itemSelection, QItemSelectionModel::Select);
        return;
    }
    if (key == Qt::Key_PageUp) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()-verticalScrollBar()->pageStep());
        return;
    }
    if (key == Qt::Key_PageDown) {
        verticalScrollBar()->setValue(verticalScrollBar()->value()+verticalScrollBar()->pageStep());
        return;
    }

    if (event->text().size() == 1) {
        QChar c = event->text().at(0).toLower();
        if (!c.isSpace() && c.isPrint()) {
            auto nextIndex = mModel->findNext(c, mCurrentIndex);
            if (nextIndex.isValid()) {
                clearSelection();
                mCurrentIndex=nextIndex;
                selectionModel()->select(mCurrentIndex, QItemSelectionModel::Select);
                goto scrollToCurrent;
            }
        }
        return;
    }
    if (!arrowKeys.contains(key))
        return;

    if (event->modifiers().testFlag(Qt::AltModifier)) {
        if (key == Qt::Key_Up) {
            emit signalAction("up");
        }
        else if (key == Qt::Key_Right) {
            emit signalAction("next");
        }
        else if (key == Qt::Key_Left) {
            emit signalAction("back");
        }
        else {
            if (selectionModel()->selection().count() != 1 || !mCurrentIndex.isValid())
                return;
            emit openIndex(mCurrentIndex);
            mCurrentIndex = QModelIndex();
        }
        return;
    }

    if (!mCurrentIndex.isValid()) {
        auto index = mModel->index(0,0, mModel->filesIndex());
        if (index.isValid()) {
            clearSelection();
            selectionModel()->select(index, QItemSelectionModel::Select);
            mCurrentIndex = index;
        }
    }
    else {
        QModelIndex index;
        if (key == Qt::Key_Up) {
            index = mModel->index(mCurrentIndex.row()-1, mCurrentIndex.column(), mModel->filesIndex());
        }
        else if (key == Qt::Key_Down) {
            index = mModel->index(mCurrentIndex.row()+1, mCurrentIndex.column(), mModel->filesIndex());
        }
        else if (key == Qt::Key_Left) {
            int row = mCurrentIndex.row();
            int column = mCurrentIndex.column();
            column--;
            if (column<0) {
                column = mModel->columnCount(mModel->filesIndex())-1;
                row--;
            }
            index = mModel->index(row, column, mModel->filesIndex());
        }
        else {
            int row = mCurrentIndex.row();
            int column = mCurrentIndex.column();
            column++;
            if (column>=mModel->columnCount(mModel->filesIndex())) {
                column = 0;
                row++;
            }
            index = mModel->index(row, column, mModel->filesIndex());
        }

        auto mimeType = mModel->data(index, ScoutModel::MimeTypeRole).toString();
        if (mimeType.isEmpty())
            return;

        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            selectRange(mSelectionStartIndex, index);
        }
        else {
            clearSelection();
            selectionModel()->select(index, QItemSelectionModel::Select);
            mSelectionStartIndex = index;
        }
        mCurrentIndex = index;
    }

    scrollToCurrent:
    if (!viewport()->rect().contains(visualRect(mCurrentIndex), false)) {
        scrollTo(mCurrentIndex);
    }
}

void FilesTableView::leaveEvent(QEvent *event)
{
    QTableView::leaveEvent(event);
    auto itemDelegate = qobject_cast<FileViewDelegate*>(this->itemDelegate());
    if (itemDelegate != nullptr) {
        auto lastIndex = itemDelegate->lastPointedIndex();
        if (lastIndex.isValid())
            model()->dataChanged(lastIndex, lastIndex, {});
    }
}

void FilesTableView::selectRange(QModelIndex &index1, QModelIndex &index2)
{
    QModelIndex firstIndex, lastIndex;
    if (index1.row() < index2.row() || (index1.row() == index2.row() && index1.column() < index2.column())) {
        firstIndex = index1;
        lastIndex = index2;
    }
    else {
        firstIndex = index2;
        lastIndex = index1;
    }
    auto currentIndex = mCurrentIndex;
    clearSelection();
    mCurrentIndex = currentIndex;
    if (firstIndex.row() == lastIndex.row()) {
        for (int c = firstIndex.column(); c<=lastIndex.column(); c++) {
            auto index = mModel->index(firstIndex.row(), c, mModel->filesIndex());
            selectionModel()->select(index, QItemSelectionModel::Select);
        }
        return;
    }
    int columnCount = mModel->columnCount(mModel->filesIndex());
    for (int c = firstIndex.column(); c<columnCount; c++) {
        auto index = mModel->index(firstIndex.row(), c, mModel->filesIndex());
        selectionModel()->select(index, QItemSelectionModel::Select);
    }
    for (int r = firstIndex.row()+1; r<lastIndex.row(); r++) {
        for (int c = 0; c<columnCount; c++) {
            auto index = mModel->index(r, c, mModel->filesIndex());
            selectionModel()->select(index, QItemSelectionModel::Select);
        }
    }
    for (int c = 0; c<=lastIndex.column(); c++) {
        auto index = mModel->index(lastIndex.row(), c, mModel->filesIndex());
        selectionModel()->select(index, QItemSelectionModel::Select);
    }
}

QString FilesTableView::getSavePath()
{
    static QString sLastPath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString localDir = QFileDialog::getExistingDirectory(this,
                                                         tr("Choose destination directory"),
                                                         sLastPath,
                                                         QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
    if (localDir.isEmpty())
        return "";
    int index = localDir.lastIndexOf('/');
    if (index >= 0)
        sLastPath = localDir.mid(0, index);
    localDir+="/";
    return localDir;
}

void FilesTableView::setRowCount(int count)
{
    mRowHeight = QVector<int>(count, 100);
}

void FilesTableView::onSelectionChanged()
{
    if (selectionModel()->selection().count() == 0)
        mCurrentIndex = QModelIndex();
}

void FilesTableView::onCloseContextMenu()
{
    if (mMenu == nullptr)
        return;
    mMenu->deleteLater();
    mMenu->disconnect();
    mMenu = nullptr;
    mCloseMenuTime = QDateTime::currentDateTime();
}

void FilesTableView::actionUpload()
{
    emit signalAction("upload");
}

void FilesTableView::actionCreateDirectory()
{
    QString newName, errorMessage;
    bool ok = true;
    while (ok) {
        QString text;
        if (errorMessage.isEmpty())
            text = tr("Enter new directory name");
        else
            text = QString("<font color='red'>%1</font><br>").arg(errorMessage) + tr("Enter new directory name");
        newName = QInputDialog::getText(this, QApplication::applicationName(), text, QLineEdit::Normal, "", &ok);
        if (newName.isEmpty()) {
            errorMessage = tr("Name cannot be empty");
            continue;
        }
        if (newName=="." || newName == "..") {
            errorMessage = tr("Name cannot be '.' or '..'");
            continue;
        }
        if (newName.contains('/')) {
            errorMessage = tr("Name cannot contains /");
            continue;
        }
        errorMessage.clear();
        break;
    }
    if (!ok)
        return;

    QString newDir = mModel->currentPath()+newName;
    ProgressDialog dialog(tr("Creating directory '%1'").arg(newName), false, this);
    dialog.setWindowModality(Qt::WindowModal);
    clearSelection();
    dialog.show();
    mModel->mkdir(mModel->currentVolume(), newDir);
    dialog.close();
    mModel->refresh();
}

QMimeData *FilesTableView::selectionToMimeData() const
{
    ScoutMimeData *mimeData = new ScoutMimeData();
    connect(mimeData, &ScoutMimeData::requestDownload, this, &FilesTableView::dropToDesktop, Qt::QueuedConnection);
    QList<QUrl> list;
    QString cluster = mModel->cluster()->auth().clusterName();
    QString volume = mModel->currentVolume();
    QString urlBegin = QString("sx://%1/%2").arg(cluster).arg(volume);
    QStringList textLines;
    foreach (auto index, selectionModel()->selectedIndexes()) {
        QString file = index.data(ScoutModel::FullPathRole).toString();
        if (file.isEmpty())
            continue;
        QUrl url(urlBegin+file);
        list.append(url);
        textLines.append(urlBegin+file);
    }
#ifdef Q_OS_MAC
    mimeData->setData("promisedFilesTypes", "file");
#endif
    mimeData->setUrls(list);
    mimeData->setText(textLines.join("\n"));
    mimeData->setData("sxscout/clusterName", cluster.toUtf8());
    mimeData->setData("sxscout/volume", volume.toUtf8());
    mimeData->setData("sxscout/rootDirectory", mModel->currentPath().toUtf8());
    return mimeData;
}

bool FilesTableView::canPaste() const
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    return canPaste(mimeData, mModel->currentVolume(), mModel->currentPath());
}

bool FilesTableView::canPaste(const QMimeData *mimeData, const QString &targetVolume, const QString &targetPath) const
{
    if (mimeData == nullptr)
        return false;
    if (targetVolume.isEmpty() || targetPath.isEmpty())
        return  false;

    QStringList formats = {"sxscout/clusterName", "sxscout/volume", "sxscout/rootDirectory", "sxscout/operationType"};
    foreach (auto format, formats) {
        if (!mimeData->hasFormat(format))
            goto testDesktopFiles;
    }
    {
        QString operation = QString::fromUtf8(mimeData->data("sxscout/operationType"));
        if (operation != "move" && operation != "copy")
            return false;
        auto cluster = mModel->cluster()->auth().clusterName().toUtf8();
        if (mimeData->data("sxscout/clusterName") != cluster)
            return false;
        auto rootDir = mimeData->data("sxscout/rootDirectory");
        QString volume = QString::fromUtf8(mimeData->data("sxscout/volume"));
        if (volume == targetVolume && rootDir == targetPath.toUtf8())
            return false;

        auto srcFilter = SxFilter::getActiveFilter(mModel->cluster()->getSxVolume(volume));
        auto dstFilter = SxFilter::getActiveFilter(mModel->cluster()->getSxVolume(mModel->currentVolume()));
        if (srcFilter != nullptr || dstFilter != nullptr) {
            if (srcFilter != nullptr)
                delete srcFilter;
            if (dstFilter != nullptr)
                delete dstFilter;
            if (volume != mModel->currentVolume())
                return false;
        }

        auto files = mimeData->urls();
        if (files.isEmpty())
            return false;
        QString urlBegin = QString("sx://%1/%2/").arg(QString::fromUtf8(cluster)).arg(volume);
        foreach (auto url, files) {
            if (!url.toString().startsWith(urlBegin))
                return false;
        }
        if (volume == mModel->currentVolume()) {
            foreach (auto url, files) {
                QString file = url.toString().mid(urlBegin.length()-1);
                if (!file.endsWith("/"))
                    continue;
                if (targetPath.startsWith(file))
                    return false;
            }
        }
    }
    return true;
    testDesktopFiles:
    if (!mimeData->hasFormat("text/uri-list"))
        return false;
    auto files = mimeData->urls();
    if (files.isEmpty())
        return false;
    int index = files.first().toString().lastIndexOf('/');
    QString rootDir = files.first().toString().mid(7, index-6);
    QString urlBegin = "file://"+rootDir;
    foreach (auto url, files) {
        if (!url.toString().startsWith(urlBegin))
            return false;
    }
    return true;
}

void FilesTableView::paste(const QMimeData *mimeData, const QString &dstVolume, const QString &dstDir)
{
    auto urlList = mimeData->urls();
    if (urlList.isEmpty())
        return;
    QString scheme = urlList.first().scheme();
    if (scheme == "sx") {
        if (mPasteInfo != nullptr)
            delete mPasteInfo;
        mPasteInfo = new PasteInfo();

        mPasteInfo->move = QString::fromUtf8(mimeData->data("sxscout/operationType")) == "move";
        QString cluster = mModel->cluster()->auth().clusterName();
        mPasteInfo->volume = QString::fromUtf8(mimeData->data("sxscout/volume"));
        mPasteInfo->rootDir = QString::fromUtf8(mimeData->data("sxscout/rootDirectory"));
        QString urlBegin = QString("sx://%1/%2/").arg(cluster).arg(mPasteInfo->volume);
        int len = urlBegin.length()-1;
        foreach (auto url, urlList) {
            mPasteInfo->files.append(url.toString().mid(len));
        }
        mPasteInfo->dstVolume = dstVolume;
        mPasteInfo->dstDir = dstDir;
        QTimer::singleShot(1, this, SLOT(slotPaste()));
    }
    else if (scheme == "file") {
        QString first = urlList.first().toString();
        int index;
        if (first.endsWith('/'))
            index = urlList.first().toString().lastIndexOf('/', first.length()-2);
        else
            index = urlList.first().toString().lastIndexOf('/');
#ifdef Q_OS_WIN
        QString rootDir = first.mid(8, index-8);
#else
        QString rootDir = first.mid(7, index-7);
#endif
        QStringList paths;
        foreach (auto url, urlList) {
#ifdef Q_OS_WIN
            QString path = url.toString().mid(8);
#else
            QString path = url.toString().mid(7);
#endif
            QFileInfo fileInfo(path);
            if (fileInfo.isDir() || fileInfo.isBundle())
                path+="/";
            paths.append(path.mid(rootDir.length()+1));
        }

        ProgressDialog dialog("calculating items", true, this);
        dialog.setWindowModality(Qt::WindowModal);
        clearSelection();
        dialog.show();
        connect(&dialog, &ProgressDialog::cancelTask, mModel, &ScoutModel::abort, Qt::DirectConnection);
        connect(mModel, &ScoutModel::signalProgress, [this, &dialog](const QString &file, int progress, int count) {
            dialog.setText(tr("Uploading: %1").arg(file));
            dialog.setProgress(progress, count);
        });
        mModel->uploadFiles(rootDir, paths, dstVolume, dstDir);
        disconnect(mModel, &ScoutModel::signalProgress, 0, 0);
        dialog.close();
        mModel->refresh();
    }
}

void FilesTableView::dropToDesktop(const QString &volume, const QString &remoteDir, const QStringList &files, const QString &localDir)
{
    ProgressDialog dialog("calculating items", true, this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.show();
    connect(&dialog, &ProgressDialog::cancelTask, mModel, &ScoutModel::abort, Qt::DirectConnection);
    connect(mModel, &ScoutModel::signalProgress, [this, &dialog](const QString &file, int progress, int count) {
        dialog.setText(tr("Downloading: %1").arg(file));
        dialog.setProgress(progress, count);
    });
    mModel->downloadFiles(volume, remoteDir, files, localDir);
    disconnect(mModel, &ScoutModel::signalProgress, 0, 0);
    dialog.close();
}

void FilesTableView::actionPaste()
{
    if (!canPaste())
        return;
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    paste(mimeData, mModel->currentVolume(), mModel->currentPath());
}

void FilesTableView::actionCopy()
{
    QMimeData *mimeData = selectionToMimeData();
    mimeData->setData("sxscout/operationType", "copy");
    QApplication::clipboard()->setMimeData(mimeData);
}

void FilesTableView::actionCut()
{
    QMimeData *mimeData = selectionToMimeData();
    mimeData->setData("sxscout/operationType", "move");
    QApplication::clipboard()->setMimeData(mimeData);
}

void FilesTableView::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    if (topLeft.parent()!=mModel->filesIndex())
        return;
    if (bottomRight.parent()!=mModel->filesIndex())
        return;
    if (roles == QVector<int>({ScoutModel::NameRole, ScoutModel::FullPathRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole, ScoutModel::MimeTypeRole})) {
        int rowCount = mModel->rowCount(mModel->filesIndex());
        setRowCount(rowCount);
    }
}

void FilesTableView::scroll()
{
    verticalScrollBar()->setValue(verticalScrollBar()->value() + mScrollSpeed);
}

void FilesTableView::slotPaste()
{
    if (mPasteInfo == nullptr)
        return;
    ProgressDialog dialog("calculating items", true, this);
    dialog.setWindowModality(Qt::WindowModal);
    clearSelection();
    dialog.show();
    connect(&dialog, &ProgressDialog::cancelTask, mModel, &ScoutModel::cancelClusterTask);
    if (mPasteInfo->move) {
        connect(mModel, &ScoutModel::signalProgress, [this, &dialog](const QString &file, int progress, int count) {
            dialog.setText(tr("Moving: %1").arg(file));
            dialog.setProgress(progress, count);
        });
        mModel->moveFiles(mPasteInfo->volume, mPasteInfo->rootDir, mPasteInfo->files, mPasteInfo->dstVolume, mPasteInfo->dstDir);
    }
    else {
        connect(mModel, &ScoutModel::signalProgress, [this, &dialog](const QString &file, int progress, int count) {
            dialog.setText(tr("Copying: %1").arg(file));
            dialog.setProgress(progress, count);
        });
        mModel->copyFiles(mPasteInfo->volume, mPasteInfo->rootDir, mPasteInfo->files, mPasteInfo->dstVolume, mPasteInfo->dstDir);
    }
    disconnect(mModel, &ScoutModel::signalProgress, 0, 0);
    dialog.close();
    mModel->refresh();

    delete mPasteInfo;
    mPasteInfo = nullptr;
}

void FilesTableView::resizeRowHeight(int row, int height)
{
    if (row < 0 || row >= mRowHeight.count())
        return;
    if (mRowHeight.at(row) < height) {
        mRowHeight[row] = height;
        setRowHeight(row, height);
    }
}

void FilesTableView::setSelectionModel(QItemSelectionModel *model)
{
    auto oldModel = selectionModel();
    if (oldModel != nullptr) {
        disconnect(oldModel, &QItemSelectionModel::selectionChanged, this, &FilesTableView::onSelectionChanged);
    }
    QTableView::setSelectionModel(model);
    connect(model, &QItemSelectionModel::selectionChanged, this, &FilesTableView::onSelectionChanged);
}

int FilesTableView::sizeHintForRow(int row) const
{
    if (row<0 || row>= mRowHeight.count())
        return 100;
    return mRowHeight.at(row);
}

int FilesTableView::sizeHintForColumn(int column) const
{
    Q_UNUSED(column);
    return 150;
}


void FilesTableView::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void FilesTableView::dragMoveEvent(QDragMoveEvent *event)
{
    QString path = mModel->currentPath();
    auto index = indexAt(event->pos());
    bool highlight = false;
    auto itemDelegate = qobject_cast<FileViewDelegate*>(this->itemDelegate());
    if (itemDelegate != nullptr) {
        itemDelegate->setDragTarget(QModelIndex());
        auto lastIndex = itemDelegate->lastPointedIndex();
        if (lastIndex.isValid()) {
            update(lastIndex);
            QString type = index.data(ScoutModel::MimeTypeRole).toString();
            if (lastIndex == index) {
                if (type == "Directory") {
                    highlight = true;
                    path = index.data(ScoutModel::FullPathRole).toString();
                }
                else if (type.startsWith("Volume")) {
                    highlight = true;
                    path = index.data(ScoutModel::NameRole).toString();
                }
            }
        }
    }
    bool canPaste = false;
    if (mModel->currentVolume().isEmpty())
        canPaste = FilesTableView::canPaste(event->mimeData(), path, "/");
    else
        canPaste = FilesTableView::canPaste(event->mimeData(), mModel->currentVolume(), path);
    if (canPaste)
        event->acceptProposedAction();
    else
        event->setAccepted(false);
    if (itemDelegate != nullptr) {
        if (canPaste && highlight)
            itemDelegate->setDragTarget(index);
    }
    if (index.isValid()) {
        update(index);
    }
}

void FilesTableView::dropEvent(QDropEvent *event)
{
    auto itemDelegate = qobject_cast<FileViewDelegate*>(this->itemDelegate());
    if (itemDelegate != nullptr)
        itemDelegate->setDragTarget(QModelIndex());
    auto index = indexAt(event->pos());
    if (index.isValid())
        update(index);

    QString path = mModel->currentPath();
    if (itemDelegate != nullptr) {
        itemDelegate->setDragTarget(QModelIndex());
        auto lastIndex = itemDelegate->lastPointedIndex();
        if (lastIndex.isValid()) {
            update(lastIndex);
            QString type = index.data(ScoutModel::MimeTypeRole).toString();
            if (lastIndex == index) {
                if (type == "Directory")
                    path = index.data(ScoutModel::FullPathRole).toString();
                else if (type.startsWith("Volume"))
                    path = index.data(ScoutModel::NameRole).toString();
            }
        }
    }

    QString volume;
    if (mModel->currentVolume().isEmpty()) {
        volume = path;
        path = "/";
    }
    else
        volume = mModel->currentVolume();
    bool canPaste = FilesTableView::canPaste(event->mimeData(), volume, path);
    if (canPaste) {
        paste(event->mimeData(), volume, path);
    }

}

void FilesTableView::setModel(QAbstractItemModel *model)
{
    mModel = qobject_cast<ScoutModel *>(model);
    QTableView::setModel(model);
    connect(mModel, &ScoutModel::dataChanged, this, &FilesTableView::onDataChanged);
}

void FilesTableView::setConfig(ScoutConfig *config)
{
    mConfig = config;
}

void FilesTableView::clearCurrentIndex()
{
    mCurrentIndex = QModelIndex();
}

void FilesTableView::actionOpen()
{
    emit openIndex(mCurrentIndex);
}

void FilesTableView::actionDownload()
{
    QString localDir = getSavePath();
    if (localDir.isEmpty())
        return;
    QString volume = mModel->currentVolume();
    QStringList selectedFiles;
    QString currentDir = mModel->currentPath();
    foreach (auto index, selectionModel()->selectedIndexes()) {
        QString name = index.data(ScoutModel::FullPathRole).toString().mid(currentDir.length());
        selectedFiles.append(name);
    }
    mModel->requestDownload(volume, currentDir, selectedFiles, localDir);
    emit showTaskDialog();
}

void FilesTableView::actionRename()
{
    QString oldName = mCurrentIndex.data(ScoutModel::NameRole).toString();
    QString errorMessage;
    QString newName;
    bool ok = true;
    while (ok) {
        QString text;
        if (errorMessage.isEmpty())
            text = tr("Enter new name");
        else
            text = QString("<font color='red'>%1</font><br>").arg(errorMessage) + tr("Enter new name");
        newName = QInputDialog::getText(this, QApplication::applicationName(), text, QLineEdit::Normal, oldName, &ok);
        if (newName.isEmpty()) {
            errorMessage = tr("Name cannot be empty");
            continue;
        }
        if (newName=="." || newName == "..") {
            errorMessage = tr("Name cannot be '.' or '..'");
            continue;
        }
        if (newName.contains('/')) {
            errorMessage = tr("Name cannot contains /");
            continue;
        }
        errorMessage.clear();
        break;
    }
    if (!ok)
        return;
    QString source = mCurrentIndex.data(ScoutModel::FullPathRole).toString();
    QString destination;
    if (source.endsWith("/")) {
        int index = source.lastIndexOf('/', source.length()-2);
        if (index==-1)
            return;
        destination = source.mid(0, index+1)+newName+"/";
    }
    else {
        int index = source.lastIndexOf('/');
        if (index==-1)
            return;
        destination = source.mid(0, index+1)+newName;
    }
    ProgressDialog dialog(tr("Renaming '%1' to '%2'").arg(oldName).arg(newName), false, this);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.show();
    clearSelection();
    mModel->rename(mModel->currentVolume(), source, destination);
    dialog.close();
    mModel->refresh();
}

void FilesTableView::actionRemove()
{
    QString volume = mModel->currentVolume();
    auto selectedIndexes = selectionModel()->selectedIndexes();
    QString message;
    if (selectedIndexes.count() == 1) {
        QString mimeType = selectedIndexes.first().data(ScoutModel::MimeTypeRole).toString();
        QString name = selectedIndexes.first().data(ScoutModel::NameRole).toString();
        if (mimeType == "Directory")
            message = tr("Do you want to remove directory %1?").arg(name);
        else
            message = tr("Do you want to remove file %1?").arg(name);
    }
    else {
        message = tr("Do you want to remove %1 elements?").arg(selectedIndexes.count());
    }
    int result = QMessageBox::question(this, QApplication::applicationName(), message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result == QMessageBox::Yes) {
        QStringList filesToRemove;
        foreach (auto index, selectedIndexes) {
            filesToRemove.append(index.data(ScoutModel::FullPathRole).toString());
        }
        ProgressDialog dialog("calculating items", true, this);
        dialog.setWindowModality(Qt::WindowModal);
        dialog.show();
        clearSelection();
        connect(mModel, &ScoutModel::signalProgress, [this, &dialog](const QString &file, int progress, int count) {
            dialog.setText(tr("Removing: %1").arg(file));
            dialog.setProgress(progress, count);
        });
        connect(&dialog, &ProgressDialog::cancelTask, mModel, &ScoutModel::cancelClusterTask);
        mModel->removeFiles(volume, filesToRemove);
        disconnect(mModel, &ScoutModel::signalProgress, 0, 0);
        dialog.close();
        mModel->refresh();
    }
}

void FilesTableView::actionDetails()
{
    QStringList files;
    if (selectionModel()->selectedIndexes().isEmpty())
        files.append(mModel->currentPath());
    else {
        foreach (auto index, selectionModel()->selectedIndexes()) {
            files.append(index.data(ScoutModel::FullPathRole).toString());
        }
    }
    DetailsDialog dialog(mModel, files, this);
    connect(&dialog, &DetailsDialog::openTasksDialog, this, &FilesTableView::showTaskDialog);
    dialog.exec();
}

void FilesTableView::actionPublicLink()
{
    QString path = mCurrentIndex.data(ScoutModel::FullPathRole).toString();
    ShareFileDialog dialog(mConfig->shareConfig(), mModel->sxwebAddress(), mModel->sxshareAddress(), mModel->currentVolume(), path, this);
    if (dialog.exec()) {
        QApplication::clipboard()->setText(dialog.publicLink());
        QString msg = tr("A public link to your file has been created and copied to the clipboard.<br>Click <a href='%1'>here</a> to open it in a browser.").arg(dialog.publicLink());
        QMessageBox::information(this, QApplication::applicationName(), msg);
    }
}
