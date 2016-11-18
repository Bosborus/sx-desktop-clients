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

#include "util.h"
#include <math.h>
#include <stdexcept>
#include <QMap>
#include <QStringList>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

extern "C"
{
    #include "sxfilter/crypt_blowfish.h"
}

#define BCRYPT_ITERATIONS_LOG2 14

QString makeRelativeTo(const QString& root, const QString& path)
{
    if (path.startsWith(root))
    {
        if (path == root)
        {
            return "/";
        }
        QString ret = path.mid(root.size());;
        if (ret.startsWith("/"))
            return ret;
        else
            return "/"+ret;
    }
    throw std::logic_error("Path " + path.toStdString() + " not under " + root.toStdString());
}

QString formatSize(qint64 size, int precision)
{
    double s = size;
    static QStringList units({"B", "kB", "MB", "GB", "TB", "PB"});
    int p = 0;
    while (p<units.size()-1)
    {
        if (s < 1024)
            break;
        else
        {
            s/=1024;
            p++;
        }
    }
    if (precision==-1)
    {
        if (fmod(100*s,100))
            precision = 2;
        else
            precision = 0;
    }
    if (p==0)
        precision = 0;
    return QString("%1 %2").arg(s, 0, 'f', precision).arg(units.at(p));
}

static QMap<QString, QString> mimeIcons = QMap<QString, QString>{
    {"x-office-spreadsheet", "icon-doc"},
    {"x-office-document", "icon-doc"},
    {"x-office-presentation", "icon-ppt"},
    {"image-x-generic", "icon-image"},
    {"text-html", "icon-xml"},
    {"package-x-generic", "icon-zip"},
    {"video-x-generic", "icon-movie"},
    {"audio-x-generic", "icon-music"}
};

QString builtInIconForMime(const QString& genericIconName)
{
    QString icon = mimeIcons.value(genericIconName, "icon-blank");
    if (isRetina())
        icon+="@2x";
    icon += ".png";
    return icon;
}

int versionStringToInt(const QString& ver)
{
    auto const parts = ver.split(".", QString::SkipEmptyParts);
    int verNum = 0;
    int m = 1;
    for (int i = parts.size() - 1; i>=0; i--)
    {
        verNum += m * parts.at(i).toInt();
        m = m*100;
    }
    return verNum;
}
QString versionIntToString(int ver)
{
    int a,b,c;
    a = ver / 10000;
    ver = ver % 10000;
    b = ver / 100;
    c = ver % 100;
    return QString("%1.%2.%3").arg(a).arg(b).arg(c);
}

QString deriveKey(const QString &sx_username, const QString &sx_password, const QString &sx_uuid, uint iterations)
{
    QByteArray username = sx_username.toUtf8();
    QByteArray password = sx_password.toUtf8();
    QByteArray uuid = sx_uuid.toUtf8();

    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(uuid);
    sha1.addData(username);

    QByteArray salt = sha1.result();

    char settingbuf[30], keybuf[61];
    const char *genkey, *setting;

    setting = _crypt_gensalt_blowfish_rn("$2b$", iterations, salt.constData(), salt.size(), settingbuf, sizeof(settingbuf));
    if (!setting)
    {
        qCritical() << "_crypt_gensalt_blowfish_rn failed";
        return QString();
    }

    genkey = _crypt_blowfish_rn(password.constData(), setting, keybuf, sizeof(keybuf));
    if (!genkey)
    {
        qCritical() << "_crypt_blowfish_rn failed";
        return QString();
    }

    sha1.reset();
    sha1.addData(uuid);
    sha1.addData(genkey,60);
    QByteArray salt_sha = sha1.result();

    sha1.reset();
    sha1.addData(username);
    QByteArray user_sha = sha1.result();

    QByteArray key_sha = user_sha + salt_sha + '\0' + '\0';
    QByteArray key_base64 = key_sha.toBase64();

    return key_base64;
}

QString formatEta(qint64 seconds)
{
    QString etaStr;
    int len = 0;
    if (seconds > 60*60*24*365)
    {
        qint64 years = seconds/(60*60*24*365);
        seconds = seconds%(60*60*24*365);
        etaStr = QString("%1 y").arg(years);
        len++;
    }
    if (seconds > 60*60*24)
    {
        qint64 days = seconds/(60*60*24);
        seconds = seconds%(60*60*24);
        etaStr += ((etaStr.isEmpty())?"":" ")+QString("%1 d").arg(days);
        len++;
    }
    if (seconds > 60*60 && len<3)
    {
        qint64 hours = seconds/(60*60);
        seconds = seconds%(60*60);
        etaStr += ((etaStr.isEmpty())?"":" ")+QString("%1 h").arg(hours);
        len++;
    }
    if (seconds > 60 && len<3)
    {
        qint64 minutes = seconds/60;
        seconds = seconds%60;
        etaStr += ((etaStr.isEmpty())?"":" ")+QString("%1 m").arg(minutes);
        len++;
    }
    if (len < 3)
    {
        etaStr += ((etaStr.isEmpty())?"":" ")+QString("%1 s").arg(seconds);
    }
    return etaStr;
}

static bool s_isRetina = false;

void setIsRetina(bool isRetina)
{
    s_isRetina = isRetina;
}

bool isRetina()
{
    return s_isRetina;
}

bool isSubdirectory(const QDir &dir, const QDir &subdir)
{
    QString path1 = dir.absolutePath();
    QString path2 = subdir.absolutePath();

    if (path2.startsWith(path1+"/"))
        return true;
    return false;
}

void removeVolumeFilterConfig(const QString &clusterUUID, const QString &volume)
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString volumeConfigDir = QString("%1/%2/%3").arg(cacheDir).arg(clusterUUID).arg(volume);
    QDir dir(volumeConfigDir);
    if (dir.exists())
        dir.removeRecursively();
}
