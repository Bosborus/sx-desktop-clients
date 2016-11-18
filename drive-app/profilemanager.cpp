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

#include "profilemanager.h"
#include <QApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QTemporaryFile>
#include "whitelabel.h"
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include "maincontroller.h"
#include "sxstate.h"

ProfileManager::ProfileManager(QObject *parent) :
    QObject(parent),
    m_localServer(0),
    m_mainController(0)
{
    connect(&m_futureWatcher, &QFutureWatcher<QList<ProfileStatus>>::finished, this, &ProfileManager::onFutureFinished);
}

ProfileManager *ProfileManager::instance()
{
    static ProfileManager sProfileManager;
    return &sProfileManager;
}

static QString autostartFilename(QString profile)
{
    QString filename = __applicationName;
    if (profile != "default")
        filename += "-"+profile;
#if defined Q_OS_WIN
    return filename;
#elif defined Q_OS_MAC
    QString dname = QDir::homePath() + "/Library/LaunchAgents";
    QDir d(dname);
    return d.filePath("com.skylable." + filename + ".plist");
#elif defined Q_OS_UNIX
    QString dname = getenv("XDG_CONFIG_HOME");
    if(dname.isNull())
        dname = QDir::homePath() + "/.config";
    dname += "/autostart";
    QDir d(dname);
    return d.filePath(filename+".desktop");
#else
    return "";
#endif
}

static bool autostartEnabled(QString profile)
{
    QString file = autostartFilename(profile);
#ifdef Q_OS_WIN
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return s.contains(file);
#else
    QFileInfo finfo(file);
    return finfo.isFile();
#endif
}

static QString configFilePath(QString profile) {
    QString filename = __applicationName;
    if (profile != "default")
        filename += "-"+profile;

    QSettings s;
#ifdef Q_OS_WIN
    QString sep = "\\";
#else
    QString sep = "/";
#endif
    QStringList pathList = s.fileName().split(sep);
    pathList.removeLast();
    QString path = pathList.join(sep);
#if defined Q_OS_MAC
    QStringList domainList = __organizationDomain.split(".");
    QString prefix;
    for (int i=domainList.size()-1; i>=0; i--)
        prefix += domainList[i]+".";
    return path+"/"+prefix+filename+".plist";
#elif defined Q_OS_UNIX
    return path+"/"+filename+".conf";
#elif defined Q_OS_WIN
    return path+"\\"+filename;
#else
    return "";
#endif
}

QStringList ProfileManager::listProfiles() const
{
    bool caseSensitive = true;
    QStringList list;
    QSettings s;
#ifdef Q_OS_WIN
    QString sep = "\\";
#else
    QString sep = "/";
#endif
    QStringList pathList = s.fileName().split(sep);
    pathList.removeLast();
    QString path = pathList.join(sep);

#if defined Q_OS_WIN
    QString prefix = __applicationName;
    QString sufix = "";
    QStringList configFiles;

    QSettings settings(path, QSettings::NativeFormat);

    foreach (QString key, settings.allKeys()) {
        QString profile = key.split("/").first();
        if (configFiles.isEmpty() || configFiles.last() != profile)
            configFiles.append(profile);
    }
#elif defined Q_OS_MAC
    QStringList domainList = __organizationDomain.split(".");
    QString prefix;
    for (int i=domainList.size()-1; i>=0; i--)
        prefix += domainList[i]+".";
    prefix+=__applicationName;
    QString sufix = ".plist";
    QDir dir(path);
    QStringList configFiles = dir.entryList(QStringList({prefix+"*"+sufix}));
#elif defined Q_OS_UNIX
    QString prefix = __applicationName;
    QString sufix = ".conf";
    QDir dir(path);
    QStringList configFiles = dir.entryList(QStringList({prefix+"*"+sufix}));
#endif

#ifdef Q_OS_UNIX
    QTemporaryFile tmpFile(dir.absolutePath()+"/.TMP.XXXXXX");
    if (tmpFile.open())
    {
        QFileInfo fileInfo(tmpFile.fileName().toLower());
        if (!fileInfo.exists())
            caseSensitive = false;
        tmpFile.close();
    }
#endif

    if (caseSensitive) {
        prefix = prefix.toCaseFolded();
        for (int i=0; i<configFiles.size(); i++)
            configFiles[i] = configFiles[i].toCaseFolded();
    }

    foreach (QString file, configFiles)
    {
        if (file==prefix+sufix)
            list.insert(0, "default");
        else if (file.startsWith(prefix+"-")) {
            QString tmp = file.mid(prefix.length()+1, file.length()-prefix.length()-sufix.length()-1);
            list.append(tmp);
        }
    }

    return list;
}

QStringList ProfileManager::profileStatus(QString profile)
{
    QString status = "error";
    QString autostart = "error";

    if (profile == m_profile)
    {
        status = m_mainController->getState().toString();
        if (status == "paused")
        {
            if (m_mainController->isWizardVisible())
                status = "paused-wizard";
        }
        SxConfig config(m_profile);
        autostart = config.desktopConfig().autostart() ? "enabled" : "disabled";
    }
    else
    {
        autostart = autostartEnabled(profile) ? "enabled" : "disabled";
        status = "off";
        QString serverName = getLocalServerName(profile);
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (!socket.waitForConnected(1000))
            goto endFunction;
        socket.write(QString("status").toUtf8());
        socket.flush();
        if (!socket.waitForReadyRead()) {
            status = "not responding";
            goto endFunction;
        }
        status = QString::fromUtf8(socket.readAll());
        socket.disconnectFromServer();
        if (socket.state() != QLocalSocket::UnconnectedState)
            socket.waitForDisconnected();
        }
    endFunction:
    return {status, autostart};
}

bool ProfileManager::removeProfile(QString profile)
{
    QString filename = __applicationName;
    if (profile != "default")
        filename += "-"+profile;

    QSettings s;
#ifdef Q_OS_WIN
    QString sep = "\\";
#else
    QString sep = "/";
#endif
    QStringList pathList = s.fileName().split(sep);
    pathList.removeLast();
    QString path = pathList.join(sep);

#if defined Q_OS_MAC
    QStringList domainList = __organizationDomain.split(".");
    QString prefix;
    for (int i=domainList.size()-1; i>=0; i--)
        prefix += domainList[i]+".";
    QString config = prefix+filename+".plist";
#elif defined Q_OS_UNIX
    QString config = filename+".conf";
#endif

#if defined Q_OS_WIN
    QSettings settings(path, QSettings::NativeFormat);
    settings.remove(filename);
#else
    bool failed = true;
#if defined Q_OS_MAC
    failed = false;
    if (QProcess::execute("/usr/bin/defaults", {"delete", path+"/"+config}))
        failed = true;
#endif
    QDir dir(path);
    if (!dir.remove(config) && failed)
        return false;
#endif

    QString cacheDirPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/../"+__applicationName;
    if (profile != "default")
        cacheDirPath += "-"+profile;
    QDir cacheDir = QDir(cacheDirPath);
    cacheDir.removeRecursively();

    if (!autostartEnabled(profile))
        return true;

    filename = autostartFilename(profile);
#if defined Q_OS_WIN
    QSettings s2("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    s2.remove(filename);
    if (s2.contains(filename))
        return false;
#else
    QFile file(filename);
    if (!file.remove())
        return false;
#endif
    return true;
}

void ProfileManager::setMainController(MainController *mainController)
{
    m_mainController = mainController;
    m_profile = m_mainController->profile();
}

void ProfileManager::createProfile(QString name)
{
    QString configPath = configFilePath(name);
    QSettings s(configPath, QSettings::NativeFormat);
    s.setValue("delete", "me");
    s.sync();
}

void ProfileManager::closeAllProfiles()
{
    auto profiles = listProfiles();
    foreach (auto profile, profiles) {
        QString serverName = getLocalServerName(profile);
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (!socket.waitForConnected(1000))
            continue;
        socket.write(QString("close").toUtf8());
        socket.flush();
        if (!socket.waitForReadyRead()) {
            continue;
        }
        socket.disconnectFromServer();
        if (socket.state() != QLocalSocket::UnconnectedState)
            socket.waitForDisconnected();
    }
}

QString ProfileManager::getLocalServerName(QString profile)
{
    QString name = "mainInstanceOf"+__applicationName+"-";
    if (profile.isEmpty())
        name += "default";
    else
        name += profile;
    return name;
}

static QList<ProfileManager::ProfileStatus> getProfilesStatus()
{
    QList<ProfileManager::ProfileStatus> result;
    QStringList profiles = ProfileManager::instance()->listProfiles();
    foreach (QString p, profiles) {
        QStringList status = ProfileManager::instance()->profileStatus(p);
        result.append(ProfileManager::ProfileStatus(p, status.first(), status.at(1) == "enabled"));
    }
    return result;
}

void ProfileManager::requestProfilesStatus()
{
    if (m_future.isRunning())
        return;

    m_future = QtConcurrent::run(getProfilesStatus);
    m_futureWatcher.setFuture(m_future);
}

void ProfileManager::onFutureFinished()
{
    QList<ProfileManager::ProfileStatus> result = m_future.result();
    emit gotProfilesStatus(result);
}
