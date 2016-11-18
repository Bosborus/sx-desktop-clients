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

#include "sizevalidator.h"
#include <QDebug>

SizeValidator::SizeValidator(QObject *parent)
    : QValidator (parent)
{

}

QValidator::State SizeValidator::validate(QString &input, int &pos) const
{
    Q_UNUSED(pos);
    if (input.isEmpty())
        return QValidator::Acceptable;
    qint64 size = parseSize(input);
    return size >= 0 ? QValidator::Acceptable : QValidator::Invalid;
}

qint64 SizeValidator::parseSize(const QString &input) const
{
    static QRegExp regexpFloat("^[0-9]*([\\.,][0-9]*)?");
    static QRegExp regexpSufix("\\s?[TGMK]?B?");
    double size = 0;
    qint64 mult = 1;
    bool ok = false;
    if (regexpFloat.exactMatch(input))
        size = input.toDouble(&ok);
    else {
        QString sizeStr = input.mid(0, regexpFloat.matchedLength());
        QString sufix = input.mid(regexpFloat.matchedLength()).toUpper();
        size = sizeStr.toDouble(&ok);
        bool match = regexpSufix.exactMatch(sufix);
        if (match) {
            if (sufix.contains('T'))
                mult = 1024LL*1024*1024*1024;
            else if (sufix.contains('G'))
                mult = 1024*1024*1024;
            else if (sufix.contains('M'))
                mult = 1024*1024;
            else if (sufix.contains('K'))
                mult = 1024;
        }
        else
            ok = false;
    }
    if (ok)
        return static_cast<qint64>(size*mult);
    else
        return -1;
}
