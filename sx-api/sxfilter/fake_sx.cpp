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

#include "fake_sx.h"
#include <stdarg.h>
#include <QDebug>
#include <QString>
#include "sxfilter.h"
#include <QFuture>
#include "sxlog.h"

int (*filter_get_input)(sxc_input_t type, const char* prompt, char *in, int insize) = 0;

int sxc_filter_msg(const sxf_handle_t *handle, int level, const char *format, ...)
{
    QString f(format);
    if (f.endsWith('\n'))
    {
        f.remove(f.length()-1, 1);
    }
    const SxFilter *cac = static_cast<const SxFilter*>(handle);
    SxFilter *ac =  const_cast<SxFilter*>(cac);

    va_list ap;
    va_start(ap, format);

    QString msg;
    msg = msg.vsprintf(f.toLocal8Bit().constData(), ap);

    switch (level) {
    case SX_LOG_ALERT:
    case SX_LOG_CRIT:
    case SX_LOG_ERR:
    {
        if (ac)
            ac->setError(msg);
        logError(msg);
    }break;
    case SX_LOG_WARNING:
    {
        if (ac)
            ac->setLastWarning(msg);
        logWarning(msg);
    }break;
    case SX_LOG_NOTICE: {
        if (ac)
            ac->setLastNotice(msg);
        logVerbose(msg);
    } break;
    default:
        logDebug(msg);
    }
    va_end(ap);
    return 0;
}

int sxc_filter_get_input(const sxf_handle_t *h, sxc_input_t type, const char *prompt, const char *def, char *in, unsigned int insize)
{
    sx_input_args args(type, prompt, def, in, insize);
    SxFilter *filter = const_cast<SxFilter *>(static_cast<const SxFilter*>(h));
    if (filter) {
        return filter->getInput(args);
    }
    return 1;
}


int sxc_meta_getval(sxc_meta_t *meta, const char *key, const void **value, unsigned int *value_len)
{
    auto cmeta = static_cast<SxMeta*>(meta);
    if (!cmeta || !key)
        return 1;
    QString ckey = QString::fromUtf8(key);
    if (!cmeta->contains(ckey))
        return 1;
    auto cvalue = cmeta->value(ckey);
    if (value)
        *value = cvalue.constData();
    if (value_len)
        *value_len = cvalue.length();
    return 0;
}


int sxc_meta_setval(sxc_meta_t *meta, const char *key, const void *value, unsigned int value_len)
{
    auto cmeta = static_cast<SxMeta*>(meta);
    if (!cmeta || !key || !value)
        return 1;

    QString ckey = QString::fromUtf8(key);
    auto cvalue = QByteArray((char*)value, value_len);
    cmeta->setValue(ckey, cvalue);
    return 0;
}

uint64_t sxc_file_get_size(const sxc_file_t *file)
{
    return file->size;
}

int sxi_file_set_size(sxc_file_t *file, uint64_t size)
{
    if (!file)
        return 1;
    file->size = size;
    return 0;
}
