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

#include "scoutmimedata.h"
#include <QDebug>
#include <QUrl>
#include <QDateTime>
#ifdef Q_OS_WIN
#include <QDateTime>
#include <Shlobj.h>
#include "winstorage.h"
#pragma comment(lib, "Ole32.lib")
#endif

ScoutMimeData::ScoutMimeData()
{
    qsrand(QDateTime::currentDateTime().toTime_t());
}

void ScoutMimeData::requestDownloadTo(const QString &localDir)
{
    QString cluster = QString::fromUtf8(data("sxscout/clusterName"));
    QString volume = QString::fromUtf8(data("sxscout/volume"));
    QString rootDirectory = QString::fromUtf8(data("sxscout/rootDirectory"));

    QString urlBegin = QString("sx://%1/%2/").arg(cluster).arg(volume);
    QStringList files;

    foreach (auto url, urls()) {
        QString path = url.toString();
        if (!path.startsWith(urlBegin))
            return;
        files.append(path.mid(urlBegin.length()-1));
    }
    emit requestDownload(volume, rootDirectory, files, localDir);
}

QVariant ScoutMimeData::retrieveData(const QString &mimetype, QVariant::Type preferredType) const
{
    #if defined Q_OS_WIN
    if(mimetype == "FileGroupDescriptorW")
    {
        unsigned int size = sizeof(FILEGROUPDESCRIPTOR);
        QByteArray buffer(static_cast<int>(size), 0);
        FILEGROUPDESCRIPTOR *desc = reinterpret_cast<FILEGROUPDESCRIPTOR *>(buffer.data());
        desc->cItems = 1;
        QString filename = QString(".sxscout-drop-target-%1").arg(qrand());
        desc->fgd[0].dwFlags = FD_PROGRESSUI;
        wcscpy_s(desc->fgd[0].cFileName, filename.toStdWString().c_str());
        return buffer;
    }
    #endif
    return QMimeData::retrieveData(mimetype, preferredType);
}

bool ScoutMimeData::hasFormat(const QString &mimetype) const
{
    auto f = formats();
    return f.contains(mimetype);
}

QStringList ScoutMimeData::formats() const
{
    QStringList formats = QMimeData::formats();
#if defined Q_OS_WIN
    formats.append("FileGroupDescriptorW");
    formats.append("FileContents");
#elif defined Q_OS_MAC
    formats.append("PromisedFileUrl");
    formats.append("PromisedFileContentType");
    //formats.append("PropertyList-1.0.dtd");
#endif
    return formats;
}

#ifdef Q_OS_WIN

static const QHash<int, QString> formats = {
    {CF_TEXT	,"CF_TEXT"},
    {CF_BITMAP	,"CF_BITMAP"},
    {CF_METAFILEPICT	,"CF_METAFILEPICT"},
    {CF_SYLK	,"CF_SYLK"},
    {CF_DIF	,"CF_DIF"},
    {CF_TIFF	,"CF_TIFF"},
    {CF_OEMTEXT	,"CF_OEMTEXT"},
    {CF_DIB	,"CF_DIB"},
    {CF_PALETTE	,"CF_PALETTE"},
    {CF_PENDATA	,"CF_PENDATA"},
    {CF_RIFF	,"CF_RIFF"},
    {CF_WAVE	,"CF_WAVE"},
    {CF_UNICODETEXT	,"CF_UNICODETEXT"},
    {CF_ENHMETAFILE	,"CF_ENHMETAFILE"},
    {CF_HDROP	,"CF_HDROP"},
    {CF_LOCALE	,"CF_LOCALE"},
    {CF_DIBV5	,"CF_DIBV5"},
    {CF_MAX	,"CF_MAX"},
    {CF_OWNERDISPLAY	,"CF_OWNERDISPLAY"},
    {CF_DSPTEXT	,"CF_DSPTEXT"},
    {CF_DSPBITMAP	,"CF_DSPBITMAP"},
    {CF_DSPMETAFILEPICT	,"CF_DSPMETAFILEPICT"},
    {CF_DSPENHMETAFILE	,"CF_DSPENHMETAFILE"},
    {CF_PRIVATEFIRST	,"CF_PRIVATEFIRST"},
    {CF_PRIVATELAST	,"CF_PRIVATELAST"},
    {CF_GDIOBJFIRST	,"CF_GDIOBJFIRST"},
    {CF_GDIOBJLAST	,"CF_GDIOBJLAST"},
};

WinMime::WinMime()
{
    mFileContentId = QWinMime::registerMimeType("FileContents");
    mPerformedDropEffectId = QWinMime::registerMimeType("Performed DropEffect");
}

bool WinMime::canConvertFromMime(const FORMATETC &formatetc, const QMimeData *mimeData) const
{
    Q_UNUSED(mimeData);
    if (formatetc.cfFormat == mFileContentId)
        return true;
    if (formatetc.cfFormat == mPerformedDropEffectId) {
        qDebug() << "mPerformedDropEffectId";
        return true;
    }
    return false;
}


bool WinMime::convertFromMime(const FORMATETC &formatetc, const QMimeData *mimeData, STGMEDIUM *pmedium) const
{
    if (formatetc.cfFormat != mFileContentId)
        return false;
    pmedium->tymed = TYMED_ISTORAGE;
    pmedium->pUnkForRelease = 0;

    auto cScoutMimeData = qobject_cast<const ScoutMimeData*>(mimeData);
    auto scoutMimeData = const_cast<ScoutMimeData*>(cScoutMimeData);
    pmedium->pstg = new WinStorage(scoutMimeData);

    /*
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, MAX_PATH*sizeof(WCHAR));
    void *out = GlobalLock(hGlobal);
    ZeroMemory(out, MAX_PATH*sizeof(WCHAR));
    lstrcpy(OLESTR("E://test"), (LPWSTR)out);
    GlobalUnlock(hGlobal);
    auto olestr = OLESTR("E://test");
    pmedium->hGlobal = hGlobal;
    */


    return true;
}

QVector<FORMATETC> WinMime::formatsForMime(const QString &mimeType, const QMimeData *mimeData) const
{
    Q_UNUSED(mimeType);
    Q_UNUSED(mimeData);
    QVector<FORMATETC> list;
    return list;
}

bool WinMime::canConvertToMime(const QString &mimeType, IDataObject *pDataObj) const
{
    Q_UNUSED(mimeType);
    Q_UNUSED(pDataObj);
    return false;
}

QVariant WinMime::convertToMime(const QString &mimeType, IDataObject *pDataObj, QVariant::Type preferredType) const
{
    Q_UNUSED(mimeType);
    Q_UNUSED(pDataObj);
    Q_UNUSED(preferredType);
    return QVariant{};
}

QString WinMime::mimeForFormat(const FORMATETC &formatetc) const
{
    Q_UNUSED(formatetc);
    return "";
}

#endif
