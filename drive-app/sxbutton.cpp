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

#include "sxbutton.h"
#include <QPaintEvent>
#include <QDebug>
#include <QPainter>
#include <QCursor>
#include <QBitmap>
#include <QImage>
#include <QRgb>
#include "util.h"

SxButton::SxButton(QWidget *parent, int width, int height) : QPushButton(parent)
{
    m_width = width;
    m_height = height;
}

void SxButton::setImage(QString filename)
{
    if (m_filename==filename)
        return;
    m_filename = filename;

    m_pixmap = QPixmap(filename);
    m_pixmapDown = recolorPixmap(m_pixmap, -20,-20,-20);
    m_pixmapSelected = recolorPixmap(m_pixmap, 20, 20, 20);

    m_width = m_pixmap.width();
    m_height = m_pixmap.height();
    if (filename.contains("@2x."))
    {
        m_width /= 2;
        m_height /= 2;
    }
    repaint();
}


QPixmap SxButton::recolorPixmap(const QPixmap& pixmap, int _r, int _g, int _b)
{
    QImage image = pixmap.toImage();
    QImage alpha = image.alphaChannel();

    for (int y = 0; y < image.height(); y++)
    {
        for (int x = 0; x < image.width(); x++)
        {
            QRgb color = image.pixel(x,y);
            int r = qRed(color);
            int g = qGreen(color);
            int b = qBlue(color);

            r+= _r;
            g+= _g;
            b+= _b;

            if (r<0) r=0; else if (r>=256) r=255;
            if (g<0) g=0; else if (g>=256) g=255;
            if (b<0) b=0; else if (b>=256) b=255;

            color = QColor(r,g,b).rgb();
            image.setPixel(x,y,color);
        }
    }
    image.setAlphaChannel(alpha);
    return QPixmap::fromImage(image);
}

void SxButton::paintEvent(QPaintEvent *event)
{
    QPoint mousePos = mapFromGlobal(QCursor::pos());
    bool selected = rect().contains(mousePos);

    QPainter painter(this);

    QRect drawRect = event->rect();
    if (drawRect.width() > m_width)
    {
        int margin = (drawRect.width() - m_width) / 2;
        int left = drawRect.left() + margin;
        drawRect.setLeft(left);
        drawRect.setWidth(m_width);
    }
    if (drawRect.height() > m_height)
    {
        int margin = (drawRect.height() - m_height) / 2;
        int top = drawRect.top() + margin;
        drawRect.setTop(top);
        drawRect.setHeight(m_height);
    }

    if (isEnabled()) {
    if (isDown())
        painter.drawPixmap(drawRect, m_pixmapDown);
    else if (selected)
        painter.drawPixmap(drawRect, m_pixmapSelected);
    else
        painter.drawPixmap(drawRect, m_pixmap);
    }
    else
        painter.drawPixmap(drawRect, m_pixmap);

}

QSize SxButton::sizeHint() const
{
    return QSize(m_width, m_height);
}

QSize SxButton::minimumSizeHint() const
{
    return QSize(m_width, m_height);
}

