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

#include "sxprogressbar.h"
#include <QResizeEvent>
#include <QDebug>
#include <QLabel>
#include "whitelabel.h"

SxProgressBar::SxProgressBar(QWidget *parent) : QFrame(parent)
{
    setFrameStyle(QFrame::StyledPanel);
    mMin = 0;
    mMax = 100;
    mValue = 0;
    setFixedHeight(20);
    mBar = new QFrame(this);
    mBar->setStyleSheet(QString("background-color: %1").arg(__color_background));
    mFormat = "%p%";
    mLabel = new QLabel(this);
    mLabel->setAlignment(Qt::AlignCenter);
    mLabel->setFixedHeight(20);
    _recalculate();
}

void SxProgressBar::setMinimum(int value)
{
    mMin = value;
    _recalculate();
}

void SxProgressBar::setMaximum(int value)
{
    mMax = value;
    _recalculate();
}

void SxProgressBar::setValue(int value)
{
    mValue = value;
    _recalculate();
}

int SxProgressBar::value() const
{
    return mValue;
}

void SxProgressBar::setFormat(QString format)
{
    mFormat = format;
    QString labelMessage = mFormat;
    labelMessage.replace("%p", QString::number(mPercent, 'f', 0));
    mLabel->setText(labelMessage);
}

void SxProgressBar::_recalculate()
{
    if (mMin > mMax) {
        mPercent = 0;
    }
    else if (mValue <= mMin) {
        mPercent = 0;
    }
    else if (mValue >= mMax) {
        mPercent = 100;
    }
    else {
        double len = mMax - mMin;
        double val = mValue - mMin;
        mPercent = 100*val/len;
    }
    QString labelMessage = mFormat;
    labelMessage.replace("%p", QString::number(mPercent, 'f', 0));
    mLabel->setText(labelMessage);
}

void SxProgressBar::resizeEvent(QResizeEvent *event)
{
    int barWidth = static_cast<int>(event->size().width()*mPercent/100);
    mBar->setFixedWidth(barWidth);
    mLabel->setFixedWidth(event->size().width());
}

