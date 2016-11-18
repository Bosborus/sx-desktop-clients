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

#include "sxconfig.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTextStream>
#include "sxdatabase.h"

#define SXCFG_APPNAME QCoreApplication::applicationName()
#define SXCFG_APPPATH QDir::toNativeSeparators(QCoreApplication::applicationFilePath())

const QString DesktopConfig::mSettingsGroup = "desktop";

namespace configKeys
{
//DESKTOP_CONFIG
    static const char *LANGUAGE { "language" };
    static const char *NOTIFICATIONS { "notifications" };
    static const char *USAGE_REPORTS { "usageReports" };
    static const char *DEBUG_VERBOSE{ "debugVerbose" };
    static const char *DEBUG_LOG{ "debugLog" };
    static const char *DEBUG_LEVEL { "debugLevel" };
    static const char *UPDATES { "checkUpdates" };
    static const char *BETA_UPDATES { "checkBetaUpdates" };
    static const char *LINK_EXP_TIME {"linkExpiration"};
    static const char *NOTIFY_EMAIL {"notifyEmail"};
    static const char *TRAY_ICON_MARK {"trayIconMark"};
    static const char *TRAY_ICON_MARK_COLOR {"trayIconMarkColor"};
    static const char *NEXT_SURVEY_TIME {"nextSurveyTime"};
//VOLUMES_CONFIG
    static const char *SX_VOLUME{ "sxVolume" };
    static const char *IGNORED_PATHS { "ignoredPaths" };
    static const char *LOCAL_PATH { "localPath" };
    static const char *WHITELIST {"whitelist"};
    static const char *REG_EXP {"regexp"};
}

SxConfig::SxConfig(QString profile)
{
    mProfile = profile.isEmpty()?"default":profile;
    mSettings = new QSettings();
    mClusterConfig = new ClusterConfig(*mSettings, mMutex);
    convertOldVolume();
    mDesktopConfig = new DesktopConfig(*mSettings, mProfile, mMutex);
    testVolumes();
}

SxConfig::~SxConfig()
{
    delete mClusterConfig;
    delete mDesktopConfig;
    delete mSettings;
}

bool SxConfig::isValid() const
{
    QMutexLocker locker(&mMutex);
    return mClusterConfig->mValid;
}

bool SxConfig::firstRun() const
{
    QMutexLocker locker(&mMutex);
    return !mClusterConfig->mValid;
}

QString SxConfig::profile() const
{
    QMutexLocker locker(&mMutex);
    return mProfile;
}

QStringList SxConfig::volumes() const
{
    QMutexLocker locker(&mMutex);
    QStringList list;
    mSettings->beginGroup("volumes");
    list = mSettings->childGroups();
    mSettings->endGroup();
    return list;
}

VolumeConfig SxConfig::volume(QString name) const
{
    QMutexLocker locker(&mMutex);
    return VolumeConfig(*mSettings, name, mMutex);
}

ClusterConfig &SxConfig::clusterConfig()
{
    QMutexLocker locker(&mMutex);
    return *mClusterConfig;
}

DesktopConfig &SxConfig::desktopConfig()
{
    QMutexLocker locker(&mMutex);
    return *mDesktopConfig;
}

void SxConfig::addVolumeConfig(const QString &volume, const QString &localPath)
{
    QMutexLocker locker(&mMutex);
    mSettings->setValue(VolumeConfig::_configKey(volume, configKeys::LOCAL_PATH), localPath);
    mSettings->sync();
}

void SxConfig::addVolumeConfig(const QString &volume, const QHash<QString, QVariant> &config)
{
    QMutexLocker locker(&mMutex);
    foreach (QString key, config.keys()) {
        QVariant val = config.value(key);
        mSettings->setValue(VolumeConfig::_configKey(volume, key), val);
    }
    mSettings->sync();
}

void SxConfig::removeVolumeConfig(const QString &volume)
{
    QMutexLocker locker(&mMutex);
#ifndef Q_OS_WIN
    auto volumeKeys = {
        configKeys::IGNORED_PATHS,
        configKeys::LOCAL_PATH,
        configKeys::WHITELIST,
        configKeys::REG_EXP
    };
    foreach (auto key, volumeKeys) {
        mSettings->remove(VolumeConfig::_configKey(volume, key));
    }
#else
    mSettings->beginGroup("volumes");
    mSettings->remove(volume);
    mSettings->endGroup();
#endif
    mSettings->sync();
}

void SxConfig::syncConfig()
{
    mSettings->sync();
}

void SxConfig::clear()
{
    foreach (QString key, mSettings->allKeys()) {
        mSettings->remove(key);
    }
}

void SxConfig::convertOldVolume()
{
    if (mSettings->childGroups().contains(DesktopConfig::mSettingsGroup))
        return;
    if (mSettings->contains(configKeys::SX_VOLUME)) {
        QString volume = mSettings->value(configKeys::SX_VOLUME).toString();
        SxDatabase::setOldVolumeName(volume);
        //mSettings->remove(configKeys::SX_VOLUME);

        auto volumeKeys = {
            configKeys::IGNORED_PATHS,
            configKeys::LOCAL_PATH,
            configKeys::WHITELIST,
            configKeys::REG_EXP
        };

        foreach (auto key, volumeKeys) {
            QVariant value = mSettings->value(key);
            //mSettings->remove(key);
            mSettings->setValue(VolumeConfig::_configKey(volume, key), value);
        }
    }
    mSettings->sync();
}

void SxConfig::testVolumes()
{
    foreach (QString volName, volumes()) {
        QString localPath = volume(volName).localPath();
        if (localPath.isEmpty()) {
            removeVolumeConfig(volName);
        }
    }
}



const QString DesktopConfig::_configKey(const QString key)
{
    return mSettingsGroup+"/"+key;
}

QString DesktopConfig::language() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::LANGUAGE), "en").toString();
}

void DesktopConfig::setLanguage(const QString &language)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::LANGUAGE), language);
}

bool DesktopConfig::notifications() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::NOTIFICATIONS), true).toBool();
}

void DesktopConfig::setNotifications(bool enabled)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::NOTIFICATIONS), enabled);
}

/*
bool DesktopConfig::usageReports() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::USAGE_REPORTS), false).toBool();
}
*/

bool DesktopConfig::debugLog() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::DEBUG_LOG), true).toBool();
}

void DesktopConfig::setDebugLog(bool enabled)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::DEBUG_LOG), enabled);
}

int DesktopConfig::logLevel() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::DEBUG_LEVEL), 1).toInt();
}

void DesktopConfig::setLogLevel(int level)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::DEBUG_LEVEL), level);
}

/*
int DesktopConfig::verboseLogging() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::DEBUG_VERBOSE), false).toBool();
}
*/

bool DesktopConfig::checkUpdates() const
{
    QMutexLocker locker(&mMutex);
    return (mProfile=="default")?mSettings.value(_configKey(configKeys::UPDATES), true).toBool():false;
}

bool DesktopConfig::checkBetaVersions() const
{
    QMutexLocker locker(&mMutex);
    return (mProfile=="default")?mSettings.value(_configKey(configKeys::BETA_UPDATES), false).toBool():false;
}

void DesktopConfig::setCheckUpdates(bool checkUpdates, bool betaVersions)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::UPDATES), checkUpdates);
    mSettings.setValue(_configKey(configKeys::BETA_UPDATES), checkUpdates ? betaVersions : false);
}

int DesktopConfig::linkExpirationTime() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::LINK_EXP_TIME), -1).toInt();
}

void DesktopConfig::setLinkExpirationTime(int expirationTime)
{
    QMutexLocker locker(&mMutex);
    mSettings.value(_configKey(configKeys::LINK_EXP_TIME), expirationTime);
}

QString DesktopConfig::linkNotifyEmail() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(configKeys::NOTIFY_EMAIL)).toString();
}

void DesktopConfig::setLinkNotifyEmail(const QString &email)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::NOTIFY_EMAIL), email);
}

bool DesktopConfig::autostart() const
{
#if defined Q_OS_WIN
    QSettings winCfg(_autostartFile(), QSettings::NativeFormat);
    return winCfg.contains(SXCFG_APPNAME);
#else
    QString fname = _autostartFile();
    if(!fname.isEmpty()) {
        QFile f(fname);
        return f.exists();
    }
#endif
    return false;
}

void DesktopConfig::setAutostart(bool autostart)
{
#if defined Q_OS_WIN
    QSettings winCfg(_autostartFile(), QSettings::NativeFormat);
    if(autostart) {
        if (mProfile == "default")
            winCfg.setValue(SXCFG_APPNAME, SXCFG_APPPATH);
        else
            winCfg.setValue(SXCFG_APPNAME, SXCFG_APPPATH+" --profile "+mProfile);
    }
    else {
        winCfg.remove(SXCFG_APPNAME);
    }
#elif defined Q_OS_MAC
    QString fname = _autostartFile();
    if(!fname.isNull())	{
        QFile f(fname);
        if(autostart) {
            if(f.open(QIODevice::WriteOnly)) {
                QTextStream strm(&f);
                strm << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
                strm << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
                strm << "<plist version=\"1.0\">";
                strm << "<dict>\n";
                strm << "   <key>Label</key>\n";
                strm << "   <string>com.skylable." << SXCFG_APPNAME << "</string>\n";
                strm << "   <key>ProgramArguments</key>\n";
                strm << "   <array>\n";
                strm << "	<string>" << SXCFG_APPPATH << "</string>\n";
                if (mProfile != "default") {
                    strm << "	<string>--profile</string>\n";
                    strm << "	<string>" << mProfile << "</string>\n";
                }
                strm << "   </array>\n";
                strm << "   <key>LimitLoadToSessionType</key>\n";
                strm << "   <string>Aqua</string>\n";
                strm << "   <key>RunAtLoad</key>\n";
                strm << "   <true/>\n";
                strm << "</dict>\n";
                strm << "</plist>\n";
            }
        }
        else
            f.remove();
    }
#elif defined Q_OS_UNIX
    QString fname = _autostartFile();
    if(fname.isEmpty()) {
        return;
    }
    QFile file(fname);
    if (autostart) {
        if(file.open(QIODevice::WriteOnly)) {
            QTextStream strm(&file);
            strm << "[Desktop Entry]\n";
            strm << "Type=Application\n";
            strm << "Name=" << SXCFG_APPNAME << "\n";
#ifdef MAKE_DEB
            strm << "Exec=" << SXCFG_APPPATH << ".sh";
#else
            strm << "Exec=" << SXCFG_APPPATH;
#endif
            if (mProfile == "default")
                strm << "\n";
            else
                strm << " --profile " << mProfile << "\n";
            strm << "Terminal=false\n";
        }
    }
    else
        file.remove();
#endif
}

QPair<QString, QString> DesktopConfig::trayIconMark() const
{
    QMutexLocker locker(&mMutex);
    QString shape = mSettings.value(_configKey(configKeys::TRAY_ICON_MARK)).toString();
    QString color = mSettings.value(_configKey(configKeys::TRAY_ICON_MARK_COLOR)).toString();
    return QPair<QString, QString>(shape, color);
}

void DesktopConfig::setTrayIconMark(QString shape, QString colorName)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::TRAY_ICON_MARK), shape);
    mSettings.setValue(_configKey(configKeys::TRAY_ICON_MARK_COLOR), colorName);
}

QDateTime DesktopConfig::nextSurveyTime() const
{
    QMutexLocker locker(&mMutex);
    static const int firstSurveyDelay = 10*60;
    return mSettings.value(_configKey(configKeys::NEXT_SURVEY_TIME), QDateTime::currentDateTime().addMSecs(firstSurveyDelay*1000)).toDateTime();
}

void DesktopConfig::setNextSurveyTime(const QDateTime &time)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(configKeys::NEXT_SURVEY_TIME), time);
}

QString DesktopConfig::_autostartFile() const
{
#if defined Q_OS_WIN
    return "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
#elif defined Q_OS_MAC
    QString dname = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if(dname.isNull())
        return dname;
    dname += "/Library/LaunchAgents";
    QDir dir(dname);
    if(!dir.exists())
        dir.mkdir(dname);
    dname += "/com.skylable." + SXCFG_APPNAME + ".plist";
    return dname;
#elif defined Q_OS_UNIX
    QString dname = getenv("XDG_CONFIG_HOME");
    if(dname.isNull()) {
        dname = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        if(dname.isNull())
            return dname;
        dname += "/.config";
    }
    dname += "/autostart";
    QDir d(dname);

    if(!d.exists() && !d.mkpath(d.absolutePath()))
        return QString();
    return dname + "/" + SXCFG_APPNAME + ".desktop";
#else
    return "";
#endif
}

DesktopConfig::DesktopConfig(QSettings &settings, const QString& profile, QMutex &mutex)
    : mSettings(settings), mProfile(profile), mMutex(mutex)
{
    auto desktopKeys = {configKeys::LANGUAGE,
                        configKeys::NOTIFICATIONS,
                        configKeys::USAGE_REPORTS,
                        configKeys::DEBUG_LOG,
                        configKeys::DEBUG_VERBOSE,
                        configKeys::UPDATES,
                        configKeys::BETA_UPDATES,
                        configKeys::LINK_EXP_TIME,
                        configKeys::NOTIFY_EMAIL};
    if (!mSettings.childGroups().contains(DesktopConfig::mSettingsGroup)) {
        foreach (auto key, desktopKeys) {
            if (mSettings.contains(key)) {
                QVariant value = mSettings.value(key);
                //mSettings.remove(key);
                mSettings.setValue(mSettingsGroup+"/"+key, value);
            }
        }
        mSettings.sync();
    }
}

QString VolumeConfig::name() const
{
    QMutexLocker locker(&mMutex);
    return mVolumeName;
}

QString VolumeConfig::localPath() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(mVolumeName, configKeys::LOCAL_PATH)).toString();
}

QStringList VolumeConfig::ignoredPaths() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(mVolumeName, configKeys::IGNORED_PATHS)).toStringList();
}

bool VolumeConfig::whitelistMode() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(_configKey(mVolumeName, configKeys::WHITELIST), false).toBool();
}

QList<QRegExp> VolumeConfig::regExpList() const
{
    QMutexLocker locker(&mMutex);
    QList<QRegExp> list;
    auto tmp = mSettings.value(_configKey(mVolumeName, configKeys::REG_EXP)).toList();
    foreach (auto value, tmp) {
        QRegExp regexp = value.toRegExp();
        if (!regexp.isEmpty())
            list.append(regexp);
    }
    return list;
}

VolumeConfig::VolumeConfig(const VolumeConfig &other)
    : mSettings(other.mSettings), mMutex(other.mMutex)
{
    mVolumeName = other.mVolumeName;
}

void VolumeConfig::setSelectiveSync(const QStringList &ignoredPaths, bool whitelist, const QList<QRegExp> &regexpList)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(_configKey(mVolumeName, configKeys::IGNORED_PATHS), ignoredPaths);
    mSettings.setValue(_configKey(mVolumeName, configKeys::WHITELIST), whitelist);
    QList<QVariant> list;
    foreach (auto regexp, regexpList) {
        list.append(regexp);
    }
    mSettings.setValue(_configKey(mVolumeName, configKeys::REG_EXP), list);
}

bool VolumeConfig::isPathIgnored(const QString &path, bool local)
{
    QString relativePath;
    if (local) {
        QFileInfo fileInfo(path);
        QFileInfo rootInfo(localPath());
        if (fileInfo.absoluteFilePath()==rootInfo.absoluteFilePath())
            return false;
        if (!fileInfo.absoluteFilePath().startsWith(rootInfo.absoluteFilePath()+"/"))
            return true;
        relativePath = fileInfo.absoluteFilePath().mid(rootInfo.absoluteFilePath().size());
    }
    else
        relativePath = path;
    foreach (QString ignoredDir, ignoredPaths()) {
        if (relativePath == ignoredDir)
            return true;
        if (relativePath.startsWith(ignoredDir+"/"))
            return true;
    }
    bool whitelist = whitelistMode();
    auto list = regExpList();
    if (whitelist && list.isEmpty())
        return true;
    foreach (QRegExp regexp, list) {
        if (regexp.exactMatch(relativePath))
            return !whitelist;
    }
    return whitelist;
}

QHash<QString, QVariant> VolumeConfig::toHashtable() const
{
    QMutexLocker locker(&mMutex);
    QHash<QString, QVariant> result;
    mSettings.beginGroup("volumes");
    mSettings.beginGroup(mVolumeName);
    auto keys = mSettings.childKeys();
    //auto list = QStringList{configKeys::IGNORED_PATHS, configKeys::LOCAL_PATH, configKeys::WHITELIST, configKeys::REG_EXP};
    foreach (auto key, keys){
        result.insert(key, mSettings.value(key));
    }
    mSettings.endGroup();
    mSettings.endGroup();
    return result;
}

VolumeConfig::VolumeConfig(QSettings &settings, const QString &name, QMutex &mutex)
    : mSettings(settings), mMutex(mutex)
{
    mVolumeName = name;
}

const QString VolumeConfig::_configKey(const QString &volume, const QString key) {
    return QString("volumes/%1/%2").arg(volume).arg(key);
}
