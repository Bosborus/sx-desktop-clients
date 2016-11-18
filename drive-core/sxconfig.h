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

#ifndef SXCONFIG_H
#define SXCONFIG_H

#include <QSettings>
#include <QMutex>
#include "clusterconfig.h"

class SxAuth;

class DesktopConfig {
public:
    QString language() const;
    void setLanguage(const QString& language);
    bool notifications() const;
    void setNotifications(bool enabled);
    bool debugLog() const;
    void setDebugLog(bool enabled);
    int logLevel() const;
    void setLogLevel(int level);
    bool checkUpdates() const;
    bool checkBetaVersions() const;
    void setCheckUpdates(bool checkUpdates, bool betaVersions);
    int linkExpirationTime() const;
    void setLinkExpirationTime(int expirationTime);
    QString linkNotifyEmail() const;
    void setLinkNotifyEmail(const QString &email);
    bool autostart() const;
    void setAutostart(bool autostart);
    QPair<QString, QString> trayIconMark() const;
    void setTrayIconMark(QString shape, QString colorName);
    QDateTime nextSurveyTime() const;
    void setNextSurveyTime(const QDateTime &time);

private:
    QString _autostartFile() const;
    static inline const QString _configKey(const QString key);
public:
    static const QString mSettingsGroup;
private:
    DesktopConfig(QSettings &settings, const QString &profile, QMutex &mutex);
    QSettings &mSettings;
    const QString &mProfile;
    QMutex &mMutex;
    friend class SxConfig;
};

class VolumeConfig {
public:
    QString name() const;
    QString localPath() const;
    QStringList ignoredPaths() const;
    bool whitelistMode() const;
    QList <QRegExp> regExpList() const;

    VolumeConfig(const VolumeConfig& other);
    void setSelectiveSync(const QStringList& ignoredPaths, bool whitelist, const QList <QRegExp>& regexpList);
    bool isPathIgnored(const QString& path, bool local);
    QHash<QString, QVariant> toHashtable() const;
private:
    VolumeConfig(QSettings &settings, const QString &name, QMutex &mutex);
    static inline const QString _configKey(const QString& volume, const QString key);

    QSettings &mSettings;
    QMutex &mMutex;
    QString mVolumeName;
    friend class SxConfig;
};

class SxConfig
{
public:
    SxConfig(QString profile);
    ~SxConfig();
    bool isValid() const;
    bool firstRun() const;
    QString profile() const;
    QStringList volumes() const;
    VolumeConfig volume(QString name) const;
    ClusterConfig& clusterConfig();
    DesktopConfig& desktopConfig();
    void addVolumeConfig(const QString &volume, const QString &localPath);
    void addVolumeConfig(const QString &volume, const QHash<QString, QVariant>& config);
    void removeVolumeConfig(const QString &volume);
    void syncConfig();
    void clear();

private:
    void convertOldVolume();
    void testVolumes();
    ClusterConfig *mClusterConfig;
    DesktopConfig *mDesktopConfig;
    QSettings *mSettings;
    QString mProfile;
    mutable QMutex mMutex;

    friend class ClusterConfig;
    friend class DesktopConfig;
    friend class VolumeConfig;
};

#endif // SXCONFIG_H
