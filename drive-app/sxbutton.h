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

#ifndef SXBUTTON_H
#define SXBUTTON_H

#include <QPushButton>
#include <QPixmap>

class SxButton : public QPushButton
{
    Q_OBJECT
public:
    explicit SxButton(QWidget *parent = 0, int width=40, int height=20);
    void setImage(QString filename);

private:
    QPixmap m_pixmap;
    QPixmap m_pixmapSelected;
    QPixmap m_pixmapDown;
    int m_width, m_height;
    QString m_filename;

    QPixmap recolorPixmap(const QPixmap &pixmap, int _r, int _g, int _b);
protected:
    void paintEvent(QPaintEvent *event);

    // QWidget interface
public:
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
};

#endif // SXBUTTON_H
