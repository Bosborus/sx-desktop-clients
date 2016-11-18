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

#include "shellextensions.h"
#include <QString>
#include <QSettings>
#include <QDebug>
#include <QApplication>
#include <QDir>
#include <QStringList>
#include "whitelabel.h"

ShellExtensions *ShellExtensions::instance()
{
    static ShellExtensions _instance;
    return &_instance;
}

void ShellExtensions::enable(bool useSxShare)
{
#ifdef Q_OS_WIN
    if (mConfig == nullptr)
        return;
    mEnabled = true;
    QString profile = mConfig->profile();

    foreach (QString volume, mConfig->volumes()) {
        QString applicationName = QApplication::applicationName() + "#" + volume;
        QString rootDirectory = mConfig->volume(volume).localPath();
        QString nativePath = rootDirectory.isEmpty()?"::":QDir::toNativeSeparators(rootDirectory) + "\\";
        QString cmd = QDir::toNativeSeparators(QApplication::applicationFilePath());
        if (!profile.isEmpty())
            cmd+= " --profile "+profile;

        QSettings key("HKEY_CURRENT_USER\\Software\\Classes\\*\\shell\\"+applicationName, QSettings::NativeFormat);
        key.setValue("MUIVerb", __applicationName);
        key.setValue("SubCommands", "");
        key.setValue("AppliesTo", "System.ItemPathDisplay:~< \""+nativePath+"\"");
        key.setValue("shell/cmd1/.", tr("Share file"));
        key.setValue("shell/cmd1/command/.", cmd+" --share \"%1\"");
        key.setValue("shell/cmd2/.", tr("Show revisions"));
        key.setValue("shell/cmd2/command/.", cmd+" --rev \"%1\"");

        if (useSxShare) {
            QSettings dir_key("HKEY_CURRENT_USER\\Software\\Classes\\Folder\\shell\\"+applicationName, QSettings::NativeFormat);
            dir_key.setValue("MUIVerb", __applicationName);
            dir_key.setValue("SubCommands", "");
            dir_key.setValue("AppliesTo", QString("System.ItemPathDisplay:~< \"%1\" AND System.ItemPathDisplay:<> \"%1\"").arg(nativePath));
            dir_key.setValue("shell/cmd1/.", tr("Share directory"));
            dir_key.setValue("shell/cmd1/command/.", cmd+" --share \"%1/\"");
        }
    }
#else
    Q_UNUSED(useSxShare)
#endif
}

void ShellExtensions::disable()
{
#ifdef Q_OS_WIN
    mEnabled = false;
    QString applicationName = QApplication::applicationName();
    QStringList toRemove = { applicationName };
    QSettings key("HKEY_CURRENT_USER\\Software\\Classes\\*\\shell", QSettings::NativeFormat);
    foreach (QString entry, key.childGroups()) {
        if (entry.startsWith(applicationName+"#"))
            toRemove.append(entry);
    }
    foreach (QString entry, toRemove) {
        key.remove(entry);
    }
    toRemove = QStringList{ applicationName };
    QSettings dir_key("HKEY_CURRENT_USER\\Software\\Classes\\Folder\\shell", QSettings::NativeFormat);
    foreach (QString entry, dir_key.childGroups()) {
        if (entry.startsWith(applicationName+"#"))
            toRemove.append(entry);
    }
    foreach (QString entry, toRemove) {
        dir_key.remove(entry);
    }
#endif
}

void ShellExtensions::setConfig(SxConfig *config)
{
    mConfig = config;
}

bool ShellExtensions::enabled() const
{
    return mEnabled;
}

ShellExtensions::ShellExtensions() : QObject(0)
{
    mConfig = nullptr;
    mEnabled = false;
}

