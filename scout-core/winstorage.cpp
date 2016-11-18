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

#include "winstorage.h"
#include <QDebug>
#include <QMutexLocker>
#include <QDir>
#include "scoutmimedata.h"

WinStorage::WinStorage(ScoutMimeData *mimeData)
{
    mRefCounter = 1;
    mMimeData = mimeData;
}

HRESULT WinStorage::QueryInterface(const IID &riid, void **ppvObject)
{
    if (ppvObject == nullptr)
        return E_POINTER;
    LPOLESTR guidStrW32;
    StringFromCLSID(riid, &guidStrW32);
    QString guidStr = QString::fromUtf16((const ushort*)guidStrW32);
    CoTaskMemFree(guidStrW32);
    if (guidStr == "{00000000-0000-0000-C000-000000000046}" || guidStr == "{0000000B-0000-0000-C000-000000000046}") {
        AddRef();
        *ppvObject = this;
    }
    else
        *ppvObject = nullptr;

    return (*ppvObject == nullptr) ? E_NOINTERFACE : S_OK;
}

ULONG WinStorage::AddRef()
{
    ++mRefCounter;
    return mRefCounter;
}

ULONG WinStorage::Release()
{
    --mRefCounter;
    qDebug() << "RELEASE" << mRefCounter;
    if (mRefCounter == 0) {
        delete this;
        return 0;
    }
    return mRefCounter;
}

HRESULT WinStorage::CreateStream(const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStream **ppstm)
{
    Q_UNUSED(pwcsName);
    Q_UNUSED(grfMode);
    Q_UNUSED(reserved1);
    Q_UNUSED(reserved2);
    Q_UNUSED(ppstm);
    return S_OK;
}

HRESULT WinStorage::OpenStream(const OLECHAR *pwcsName, void *reserved1, DWORD grfMode, DWORD reserved2, IStream **ppstm)
{
    Q_UNUSED(pwcsName);
    Q_UNUSED(grfMode);
    Q_UNUSED(reserved1);
    Q_UNUSED(reserved2);
    Q_UNUSED(ppstm);
    return S_OK;
}

HRESULT WinStorage::CreateStorage(const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStorage **ppstg)
{
    Q_UNUSED(pwcsName);
    Q_UNUSED(grfMode);
    Q_UNUSED(reserved1);
    Q_UNUSED(reserved2);
    Q_UNUSED(ppstg);
    return S_OK;
}

HRESULT WinStorage::OpenStorage(const OLECHAR *pwcsName, IStorage *pstgPriority, DWORD grfMode, SNB snbExclude, DWORD reserved, IStorage **ppstg)
{
    Q_UNUSED(pwcsName);
    Q_UNUSED(pstgPriority);
    Q_UNUSED(grfMode);
    Q_UNUSED(snbExclude);
    Q_UNUSED(reserved);
    Q_UNUSED(ppstg);
    return S_OK;
}

HRESULT WinStorage::CopyTo(DWORD ciidExclude, const IID *rgiidExclude, SNB snbExclude, IStorage *pstgDest)
{
    Q_UNUSED(ciidExclude);
    Q_UNUSED(rgiidExclude)
    Q_UNUSED(snbExclude);
    STATSTG stat;
    pstgDest->Stat(&stat, 0);

    QString full_path = QDir::fromNativeSeparators(QString::fromUtf16((const ushort*)stat.pwcsName));
    int index = full_path.lastIndexOf('/');
    qDebug() << full_path;
    if (index > 0) {
        QString dirPath = full_path.mid(0, index);
        QDir dir(dirPath);
        if (dir.exists()) {
            mMimeData->requestDownloadTo(dirPath);
        }
    }
    return -1;
}

HRESULT WinStorage::MoveElementTo(const OLECHAR *pwcsName, IStorage *pstgDest, const OLECHAR *pwcsNewName, DWORD grfFlags)
{
    Q_UNUSED(pwcsName);
    Q_UNUSED(pstgDest);
    Q_UNUSED(pwcsNewName);
    Q_UNUSED(grfFlags);

    qDebug() << Q_FUNC_INFO;
    return S_OK;
}

HRESULT WinStorage::Commit(DWORD grfCommitFlags)
{
    Q_UNUSED(grfCommitFlags);
    qDebug() << Q_FUNC_INFO;
    return S_OK;
}

HRESULT WinStorage::Revert()
{
    return S_OK;
}

HRESULT WinStorage::EnumElements(DWORD reserved1, void *reserved2, DWORD reserved3, IEnumSTATSTG **ppenum)
{
    Q_UNUSED(reserved1);
    Q_UNUSED(reserved2);
    Q_UNUSED(reserved3);
    Q_UNUSED(ppenum);
    return S_OK;
}

HRESULT WinStorage::DestroyElement(const OLECHAR *pwcsName)
{
    Q_UNUSED(pwcsName);
    return S_OK;
}

HRESULT WinStorage::RenameElement(const OLECHAR *pwcsOldName, const OLECHAR *pwcsNewName)
{
    Q_UNUSED(pwcsOldName);
    Q_UNUSED(pwcsNewName);
    return S_OK;
}

HRESULT WinStorage::SetElementTimes(const OLECHAR *pwcsName, const FILETIME *pctime, const FILETIME *patime, const FILETIME *pmtime)
{
    Q_UNUSED(pwcsName);
    Q_UNUSED(pctime);
    Q_UNUSED(patime);
    Q_UNUSED(pmtime);
    return S_OK;
}

HRESULT WinStorage::SetClass(const IID &clsid)
{
    Q_UNUSED(clsid);
    return S_OK;
}

HRESULT WinStorage::SetStateBits(DWORD grfStateBits, DWORD grfMask)
{
    Q_UNUSED(grfStateBits);
    Q_UNUSED(grfMask);
    return S_OK;
}

HRESULT WinStorage::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
{
    Q_UNUSED(pstatstg);
    Q_UNUSED(grfStatFlag);
    return S_OK;
}
