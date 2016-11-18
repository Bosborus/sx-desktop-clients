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

#ifndef REMOTEFILESTABLEVIEW_H
#define REMOTEFILESTABLEVIEW_H

#include <QItemDelegate>
#include <QTableView>
#include "scoutmodel.h"
#include "scoutconfig.h"
#include "scoutmimedata.h"

class FileViewDelegate : public QItemDelegate {
    Q_OBJECT
public:
    explicit FileViewDelegate(ScoutModel *model, QTableView* view, QObject* parent=nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QModelIndex lastPointedIndex() const;
    QModelIndex dragTargetIndex() const;
    bool isIndexActive(const QModelIndex &index) const;
    static bool canSelect(const QModelIndex &index, const QRect &itemRect, const QRect &selectionRect);
    void setDragTarget(const QModelIndex& index);
private:
    ScoutModel *mModel;
    QTableView *mView;
    static const int sMargin = 4;
    static const int sIconWidth = 64;
    static const int sIconHeight = 64;
    static const int sTextPadding = 2;
    mutable QModelIndex mPointedIndex;
    QModelIndex mDragTargetIndex;
signals:
    void setRowHeight(int row, int height) const;
};

class FilesTableView : public QTableView
{
    Q_OBJECT
public:
    explicit FilesTableView(QWidget *parent = 0);
    void setModel(QAbstractItemModel *model) override;
    void setConfig(ScoutConfig *config);
    void clearCurrentIndex();
    QString getSavePath();
    void setRowCount(int count);
signals:
    void openIndex(const QModelIndex &);
    void showTaskDialog();
    void signalAction(const QString &actionName);
public slots:
    void resizeRowHeight(int row, int height);
private slots:
    void actionOpen();
    void actionDownload();
    void actionRename();
    void actionRemove();
    void actionDetails();
    void actionPublicLink();
    void onSelectionChanged();
    void onCloseContextMenu();
    void actionUpload();
    void actionCreateDirectory();
    void actionPaste();
    void actionCopy();
    void actionCut();
    void onDataChanged(const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> &roles);
    void scroll();
    void slotPaste();
    void dropToDesktop(const QString &volume, const QString &remoteDir, const QStringList &files, const QString &localDir);

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void leaveEvent(QEvent *) override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    void selectRange(QModelIndex& index1, QModelIndex& index2);
    QMimeData* selectionToMimeData() const;
    bool canPaste() const;
    bool canPaste(const QMimeData* mimeData, const QString &volume, const QString &where) const;
    void paste(const QMimeData* mimeData, const QString &dstVolume, const QString &dstDir);

private:
    ScoutModel* mModel;
    ScoutConfig *mConfig;
    QModelIndex mCurrentIndex, mSelectionStartIndex;
    QDrag *mDrag;
    QPoint *mDragStart;
    QPoint mSelectionStart;
    QRubberBand *mSelectionFrame;
    QItemSelection mPreviousSelected;
    QMenu *mMenu;
    QDateTime mCloseMenuTime;
    QVector<int> mRowHeight;
    QTimer mScrollTimer;
    int mScrollSpeed;
    bool mClearSelectionAfterRelease;
    struct PasteInfo {
        bool move;
        QString volume;
        QString rootDir;
        QStringList files;
        QString dstVolume;
        QString dstDir;
    };
    PasteInfo *mPasteInfo;
    // QAbstractItemView interface
public:
    void setSelectionModel(QItemSelectionModel *model) override;
    int sizeHintForRow(int row) const override;
    int sizeHintForColumn(int column) const override;
};

#endif // REMOTEFILESTABLEVIEW_H
