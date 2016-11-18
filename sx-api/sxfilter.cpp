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

#include "sxfilter.h"
#include "sxfilter/filter_aes256.h"
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QThread>
#include <QCoreApplication>
#include <QTranslator>
#include "sxcluster.h"
#include "sxlog.h"

QMap<QString, sxc_filter_t*> SxFilter::m_registeredFilters;

namespace RegisterFilters {
    const bool registerAES = SxFilter::registerActiveFilter(&sxc_filter_aes256);
    const bool registerAESnew = SxFilter::registerActiveFilter(&sxc_filter_aes_256_new);
}

const QString SxFilter::configDir() const
{
    const QString cluster_uuid = m_volume->cluster()->uuid();
    const QString filter_uuid = uuid();
    const QString volume = m_volume->name();
    QString filterDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + cluster_uuid + "/" + volume + "/" + filter_uuid;
    QDir d;
    if (!d.mkpath(filterDir))
    {
        return QString();
    }
    return filterDir;
}
QString SxFilter::error() const
{
    return m_error;
}

QString SxFilter::errorTr() const
{
    return QCoreApplication::translate("ActiveFilterInput", m_error.toStdString().c_str());
}

void SxFilter::setError(const QString &error)
{
    m_error = error;
}

bool SxFilter::removeConfig()
{
    QDir d(configDir());
    if (d.exists())
    {
        bool r = true;
        QStringList files = d.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
        foreach (QString f, files) {
            QFile file(d.filePath(f));
            file.setPermissions(QFile::ReadOther | QFile::WriteOther);
            if (!file.remove())
            {
                r = false;
                logDebug("remove file " + file.fileName() + " failed with error " + file.errorString());
            }
        }
        return r;
    }
    return false;
}

SxFilter::SxFilter(sxc_filter_t *filter, SxVolume *volume) :
    m_filter(filter)
{
    m_ctx = 0;
    m_volume = volume;
    m_needFinish = false;
}

SxFilter::SxFilter(const SxFilter &other)
{
    m_filter = other.m_filter;
    m_volume = other.m_volume;

    m_ctx = 0;
    m_error = QString();
    m_lastWarning = QString();
    m_needFinish = false;
}

SxFilter::~SxFilter()
{
    m_volume = 0;
    if (m_needFinish && m_filter->data_finish)
    {
        m_filter->data_finish(this, &m_ctx, m_sxf_mode);
    }
    if (m_filter->shutdown)
    {
        m_filter->shutdown(this, m_ctx);
    }
}

SxFilter *SxFilter::getActiveFilter(SxVolume *volume)
{
    if (volume == nullptr)
        return nullptr;
    QString filter_uuid = ActiveFilterUtils::uuidFormat(volume->meta().value("filterActive").toHex());
    sxc_filter_t* filter = m_registeredFilters.value(filter_uuid, nullptr);
    if (!filter)
        return nullptr;
    return new SxFilter(filter, volume);
}

bool SxFilter::isFilterSupported(QString uuid, const QByteArray &cfg)
{
    if (m_registeredFilters.contains(uuid))
    {
        if (uuid=="35a5404d-1513-4009-904c-6ee5b0cd8634"
                || uuid=="15b0ac3c-404f-481e-bc98-6598e4577bbd") // aes filter
        {
            if (cfg.length()==16)
                return false;
            else
                return true;
        }
        return true;
    }
    else
        return false;
}

bool SxFilter::isFilterSupported(const SxVolume *volume)
{
    if (volume->meta().contains("filterActive")) {
        auto value = volume->meta().value("filterActive").toHex();
        QString filter_uuid = ActiveFilterUtils::uuidFormat(value);
        if (m_registeredFilters.contains(filter_uuid)) {
            if (filter_uuid=="35a5404d-1513-4009-904c-6ee5b0cd8634"
                    || filter_uuid=="15b0ac3c-404f-481e-bc98-6598e4577bbd") // aes filter
            {
                auto cfg = volume->meta().value(filter_uuid);
                return cfg.length() != 16;
            }
            return true;
        }
        else
            return false;
    }
    else
        return true;
}

bool SxFilter::registerActiveFilter(sxc_filter_t *filter)
{
    //logEntry(QString("registerActiveFilter %1").arg(filter->shortname));
    QString uuid = QString(filter->uuid);
    if (m_registeredFilters.contains(uuid))
        return false;
    m_registeredFilters.insert(uuid, filter);
    return true;
}

bool SxFilter::testFilterConfig(SxVolume *volume)
{
    auto filter = getActiveFilter(volume);
    if (filter == nullptr)
        return false;
    bool result = filter->dataPrepare("test", volume->customMeta(), SXF_MODE_DOWNLOAD);
    delete filter;
    return result;
}

QString SxFilter::uuid() const
{
    return QString(m_filter->uuid);
}

QString SxFilter::shortname() const
{
    return QString(m_filter->shortname);
}

QString SxFilter::shortdesc() const
{
    return QString(m_filter->shortdesc);
}

bool SxFilter::fileProcess() const
{
    return m_filter->file_process;
}

bool SxFilter::fileUpdate() const
{
    return m_filter->file_update;
}

bool SxFilter::dataProcess() const
{
    return m_filter->data_process;
}

bool SxFilter::dataPrepare() const
{
    return m_filter->data_prepare;
}

bool SxFilter::dataFinish() const
{
    return m_filter->data_finish;
}

bool SxFilter::filemetaProcess() const
{
    return m_filter->filemeta_process;
}

bool SxFilter::isAes256() const
{
    return uuid()=="35a5404d-1513-4009-904c-6ee5b0cd8634" || uuid() == "15b0ac3c-404f-481e-bc98-6598e4577bbd";
}

bool SxFilter::dataPrepare(QString path, SxMeta &customMeta, sxf_mode_t sxf_mode)
{
    Q_ASSERT(m_filter->data_prepare);
    const QByteArray cfg = m_volume->meta().value(uuid()+"-cfg");
    unsigned int cfg_len = static_cast<unsigned int>(cfg.length());
    m_needFinish = true;
    m_sxf_mode = sxf_mode;
    return !m_filter->data_prepare(this, &m_ctx, path.toLocal8Bit(), QDir::toNativeSeparators(configDir()).toLocal8Bit(), cfg.constData(), cfg_len, &customMeta, sxf_mode);
}

qint64 SxFilter::dataProcess(char *inbuff, qint64 inbuffSize, char *outbuff, qint64 outbuffSize, sxf_mode_t sxf_mode, sxf_action_t *action)
{
    Q_ASSERT(m_filter->data_process);
    m_needFinish = true;
    m_sxf_mode = sxf_mode;
    ssize_t size = m_filter->data_process(this, m_ctx, inbuff, static_cast<size_t>(inbuffSize), outbuff, static_cast<size_t>(outbuffSize), sxf_mode, action);
    return size;
}

bool SxFilter::filemetaProcess(QString name, QString &newName, bool localToRemote, qint64 &size, SxMeta &fileMeta, SxMeta &volumeMeta)
{
    sxc_file_t file_t;
    file_t.size = static_cast<quint64>(size);

    QByteArray tmpName = name.toUtf8();
    const char* src = tmpName.constData();
    char *dest=NULL;
    unsigned int nslashes = 0;
    while(src[nslashes] == '/')
            nslashes++;
    src = src + nslashes;
    const QByteArray cfg = m_volume->meta().value(uuid()+"-cfg");
    auto cfg_len = static_cast<unsigned int>(cfg.length());

    int result = m_filter->filemeta_process(this, &m_ctx, QDir::toNativeSeparators(configDir()).toLocal8Bit(), cfg.constData(), cfg_len,
                                            &file_t, localToRemote ? SXF_FILEMETA_LOCAL : SXF_FILEMETA_REMOTE, src, &dest,
                                            &fileMeta, &volumeMeta);
    size = static_cast<qint64>(file_t.size);

    if (dest) {
        newName = dest;
        free(dest);
        if (!newName.startsWith("/"))
            newName.insert(0,"/");
    }
    return !result;
}

bool SxFilter::filemetaProcess(SxFile &file, bool localToRemote)
{
    int result;
    if (localToRemote) {
        qint64 size = file.mLocalSize;
        result = filemetaProcess(file.mLocalPath, file.mRemotePath, localToRemote, size, file.mMeta, m_volume->customMeta());
    }
    else {
        file.mLocalSize = file.mRemoteSize;
        result = filemetaProcess(file.mRemotePath, file.mLocalPath, localToRemote, file.mLocalSize, file.mMeta, m_volume->customMeta());
    }
    return result;
}

void SxFilter::dataFinish(sxf_mode_t sxf_mode)
{
    Q_ASSERT(m_filter->data_finish);
    m_needFinish = false;
    m_filter->data_finish(this, &m_ctx, sxf_mode);
}

void SxFilter::setLastWarning(QString error)
{
    m_lastWarning = error;
}

QString SxFilter::lastWarning() const
{
    return m_lastWarning;
}

QString SxFilter::lastWarningTr() const
{
    return QCoreApplication::translate("ActiveFilterInput", m_lastWarning.toStdString().c_str());
}

void SxFilter::clearLastWarning()
{
    m_lastWarning = QString();
}

QString SxFilter::lastNotice() const {
    return m_lastNotice;
}

void SxFilter::setLastNotice(QString notice) {
    m_lastNotice = notice;
}

QString ActiveFilterUtils::uuidFormat(const QByteArray &uuid)
{
    if (uuid.isEmpty())
        return "";
    Q_ASSERT(uuid.length()==32);
    QString uuid_str = uuid.left(8)+"-"+uuid.mid(8,4)+"-"+uuid.mid(12,4)+"-"+uuid.mid(16,4)+"-"+uuid.mid(20);
    return uuid_str;
}
/*
static const char *aes_translations[] = {
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: Can't obtain password"),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: ERROR: Password must be at least 8 characters long"),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: Can't obtain password"),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: ERROR: Passwords don't match"),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: Enter encryption password: "),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: Enter decryption password: "),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "Invalid password"),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: Re-enter encryption password: "),
    QT_TRANSLATE_NOOP("ActiveFilterInput", "[aes256]: Enable filename encryption (introduces additional slowdown)?")
};
*/


int SxFilter::getInput(sx_input_args &args)
{
    /*
    Q_UNUSED(aes_translations);
    QString p = QCoreApplication::translate("ActiveFilterInput", (args.prompt));
    if (m_lastNotice == "First upload to the encrypted volume, set the volume password now" &&
            (p==QCoreApplication::translate("ActiveFilterInput","[aes256]: Enter encryption password: ") || p==QCoreApplication::translate("ActiveFilterInput","[aes256]: Enter decryption password: " ))) {
        p = QCoreApplication::translate("ActiveFilterInput","First upload to the encrypted volume, set the volume password now")+"\n"+p;
        m_lastNotice.clear();
    }
    if (!m_lastWarning.isEmpty())
    {
        p = "<font color=\"red\">"+QCoreApplication::translate("ActiveFilterInput", m_lastWarning.toLocal8Bit().constData())+"</font><br>"+p;
        m_lastWarning.clear();
    }
    args.prompt = p.toUtf8().constData();
    */
    return m_volume->cluster()->getInput(args);
}
