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

#ifndef ACTIVEFILTER_H
#define ACTIVEFILTER_H

#include <QMap>
#include <QString>
#include <QObject>
#include "sxmeta.h"
#include "sxvolume.h"
#include "sxfilter/fake_sx.h"
#include "sxfile.h"
#include "sxfilter/sx_input_args.h"

class SxFilter
{
public:
    SxFilter(const SxFilter& other);
    ~SxFilter();
    static SxFilter *getActiveFilter(SxVolume *volume);
    static bool isFilterSupported(QString uuid, const QByteArray& cfg=QByteArray{});
    static bool isFilterSupported(const SxVolume* volume);
    static bool registerActiveFilter(sxc_filter_t* filter);
    static bool testFilterConfig(SxVolume* volume);

    QString uuid() const;
    QString shortname() const;
    QString shortdesc() const;

    bool fileProcess() const;
    bool fileUpdate()  const;
    bool dataProcess() const;
    bool dataPrepare() const;
    bool dataFinish()  const;
    bool filemetaProcess() const;

    bool isAes256() const;

    bool dataPrepare(QString path, SxMeta &customMeta, sxf_mode_t sxf_mode);
    qint64 dataProcess(char* inbuff, qint64 inbuffSize, char* outbuff, qint64 outbuffSize, sxf_mode_t sxf_mode, sxf_action_t *action);
    bool filemetaProcess(QString name, QString &newName, bool localToRemote, qint64 &size, SxMeta &fileMeta, SxMeta &volumeMeta);
    bool filemetaProcess(SxFile& file, bool localToRemote);
    void dataFinish(sxf_mode_t sxf_mode);

    void setLastWarning(QString error);
    QString lastWarning() const;
    QString lastWarningTr() const;
    void clearLastWarning();
    QString lastNotice() const;
    void setLastNotice(QString notice);

    QString error() const;
    QString errorTr() const;
    void setError(const QString &error);
    int getInput(sx_input_args &args);

    bool removeConfig();

private:
    SxFilter(sxc_filter_t* filter, SxVolume* volume);
    const QString configDir() const;
private:
    static QMap<QString, sxc_filter_t*> m_registeredFilters;
    const sxc_filter_t* m_filter;
    void *m_ctx;
    QString m_lastWarning;
    QString m_lastNotice;
    QString m_error;
    SxVolume* m_volume;
    bool m_needFinish;
    sxf_mode_t m_sxf_mode;
};

namespace ActiveFilterUtils
{
    QString uuidFormat(const QByteArray& uuid);
}

#endif // ACTIVEFILTER_H
