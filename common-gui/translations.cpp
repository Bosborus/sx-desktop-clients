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

#include "translations.h"

#include <QDir>
#include <QLocale>

Translations::Translations()
{
    QDir dir(":/translations");
    QStringList filters;
    filters << "sxdrive_*.qm";
    foreach (QString file, dir.entryList(filters)) {
        int len = file.length() - 11;
        QString lang = file.mid(8, len);
        QLocale locale(lang);
        if (locale == QLocale::C)
            continue;
        QString nativeLanguage = locale.nativeLanguageName();
        nativeLanguage = nativeLanguage.at(0).toUpper() + nativeLanguage.toLower().mid(1);
        m_translations.insert(nativeLanguage, lang);
    }
}

Translations *Translations::instance()
{
    static Translations t;
    return &t;
}

QString Translations::languageCode(QString nativeLanguage)
{
    return m_translations.value(nativeLanguage, "en");
}

QString Translations::nativeLanguage(QString languageCode)
{
    return m_translations.key(languageCode, "English");
}

bool Translations::containsLanguage(QString languageCode)
{
    return m_translations.values().contains(languageCode);
}

QStringList Translations::availableLanguages()
{
    QStringList list = m_translations.keys();
    list.sort();
    return list;
}

