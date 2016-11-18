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

#ifndef WINSTORAGE_H
#define WINSTORAGE_H

#include "Objidl.h"
#include <QMutex>

class ScoutMimeData;

class WinStorage : public IStorage
{
public:
    WinStorage(ScoutMimeData *mimeData);
    virtual ~WinStorage() {}
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID &riid, void **ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    // IStorage interface
public:
    HRESULT STDMETHODCALLTYPE CreateStream(const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStream **ppstm) override;
    HRESULT STDMETHODCALLTYPE OpenStream(const OLECHAR *pwcsName, void *reserved1, DWORD grfMode, DWORD reserved2, IStream **ppstm) override;
    HRESULT STDMETHODCALLTYPE CreateStorage(const OLECHAR *pwcsName, DWORD grfMode, DWORD reserved1, DWORD reserved2, IStorage **ppstg) override;
    HRESULT STDMETHODCALLTYPE OpenStorage(const OLECHAR *pwcsName, IStorage *pstgPriority, DWORD grfMode, SNB snbExclude, DWORD reserved, IStorage **ppstg) override;
    HRESULT STDMETHODCALLTYPE CopyTo(DWORD ciidExclude, const IID *rgiidExclude, SNB snbExclude, IStorage *pstgDest) override;
    HRESULT STDMETHODCALLTYPE MoveElementTo(const OLECHAR *pwcsName, IStorage *pstgDest, const OLECHAR *pwcsNewName, DWORD grfFlags) override;
    HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) override;
    HRESULT STDMETHODCALLTYPE Revert() override;
    HRESULT STDMETHODCALLTYPE EnumElements(DWORD reserved1, void *reserved2, DWORD reserved3, IEnumSTATSTG **ppenum) override;
    HRESULT STDMETHODCALLTYPE DestroyElement(const OLECHAR *pwcsName) override;
    HRESULT STDMETHODCALLTYPE RenameElement(const OLECHAR *pwcsOldName, const OLECHAR *pwcsNewName) override;
    HRESULT STDMETHODCALLTYPE SetElementTimes(const OLECHAR *pwcsName, const FILETIME *pctime, const FILETIME *patime, const FILETIME *pmtime) override;
    HRESULT STDMETHODCALLTYPE SetClass(const IID &clsid) override;
    HRESULT STDMETHODCALLTYPE SetStateBits(DWORD grfStateBits, DWORD grfMask) override;
    HRESULT STDMETHODCALLTYPE Stat(STATSTG *pstatstg, DWORD grfStatFlag) override;
private:
    unsigned long mRefCounter;
    ScoutMimeData *mMimeData;
};

#endif // WINSTORAGE_H
