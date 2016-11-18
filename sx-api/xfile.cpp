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

#include "xfile.h"
#if defined(Q_OS_WIN)
#include <QDir>
#include <Windows.h>
#include <io.h>
#include <Fcntl.h>
#include <QDebug>
#include "sxlog.h"

bool XFile::openFor(XFile::xopenFor mode) {
    QString fname = QDir::toNativeSeparators(fileName());
    OVERLAPPED over;
    HANDLE fh;
    int fd;

    unsetError();
    m_error = QFileDevice::OpenError;
    fh = CreateFileW((const WCHAR *)fname.constData(),
		     (mode == forRead) ? GENERIC_READ : GENERIC_READ|GENERIC_WRITE,
		     FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		     NULL,
		     (mode == forRead) ? OPEN_EXISTING : OPEN_ALWAYS,
		     FILE_FLAG_RANDOM_ACCESS,
		     NULL);
    if(fh == INVALID_HANDLE_VALUE) {
	setErrorString("CreateFile Failed");
	return false;
    }

    memset(&over, 0, sizeof(over));
    if(!LockFileEx(fh, 
		   (mode == forRead) ? LOCKFILE_FAIL_IMMEDIATELY : LOCKFILE_FAIL_IMMEDIATELY|LOCKFILE_EXCLUSIVE_LOCK,
		   0,
		   MAXDWORD,
		   MAXDWORD,
		   &over)) {
	setErrorString("LockFile failed");
	CloseHandle(fh);
	return false;
    }

    memset(&over, 0, sizeof(over));
    /* apparently the crt only honours APPEND, TEXT and NOINHERIT (ofc docs say otherwise) */
    int osflags = (mode == forRead) ? (_O_RDONLY|_O_BINARY) : (_O_RDWR|_O_BINARY);
    if((fd = _open_osfhandle((intptr_t)fh, osflags)) == -1) {
	setErrorString("open_osfhandle failed");
	UnlockFileEx(fh, 0, MAXDWORD, MAXDWORD, &over);
	CloseHandle(fh);
	return false;
    }

    m_error = QFileDevice::NoError;
    if(!QFile::open(fd, (mode == forRead) ? QIODevice::ReadOnly : QIODevice::ReadWrite, QFileDevice::DontCloseHandle)) {
	UnlockFileEx(fh, 0, MAXDWORD, MAXDWORD, &over);
	_close(fd);
	return false;
    }

    return true;
}

void XFile::close() {
    if(!isOpen())
	return;

    int fd = handle();
    QFile::close();
    HANDLE fh = (HANDLE)_get_osfhandle(fd);
    OVERLAPPED over;

    memset(&over, 0, sizeof(over));
    UnlockFileEx(fh, 0, MAXDWORD, MAXDWORD, &over);
    _close(fd);
}

bool XFile::safeRename(const QString &_oldName, const QString &_newName) {
    QString oldName = QDir::toNativeSeparators(_oldName);
    QString newName = QDir::toNativeSeparators(_newName);

    if(MoveFileW((const WCHAR *)oldName.constData(), (const WCHAR *)newName.constData())) {
        return true;
    }
    XFile xf(newName);
    if(!xf.openFor(XFile::forWrite)) {
        logWarning(QString("safeRename failed: unable to lock file: %1").arg(newName));
	return false;
    }

    // This is as "atomic" as we can get
    int counter = 0;
    while(!ReplaceFileW((const WCHAR *)newName.constData(), (const WCHAR *)oldName.constData(), NULL, 0, NULL, NULL) && counter < 10) {
        DWORD errorCode = GetLastError();
        logWarning(QString("safeRename failed: %1 (%2 => %3)").arg(errorCode).arg(oldName).arg(newName));
        Sleep(10);
        counter++;
    }
    if (counter >= 10)
        return false;
    if (counter > 0)
        logDebug("safeRename succeed");
    return true;
}

bool XFile::makeInvisible(const QString &fileName, bool invisible) {
    DWORD attribs = GetFileAttributesW((const WCHAR *)fileName.constData());
    if(attribs == INVALID_FILE_ATTRIBUTES)
	return false;
    if (invisible)
        attribs = attribs | FILE_ATTRIBUTE_HIDDEN;
    else
        attribs = attribs & ~FILE_ATTRIBUTE_HIDDEN;
    return SetFileAttributesW((const WCHAR *)fileName.constData(), attribs) != 0;
}

#else
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

bool XFile::openFor(XFile::xopenFor mode) {
    enum QIODevice::OpenModeFlag qmode = (mode == forRead) ? QIODevice::ReadOnly : QIODevice::ReadWrite;
    struct flock lock;

    m_error = QFileDevice::NoError;
    if(!QFile::open(qmode))
	return false;

    memset(&lock, 0, sizeof(lock));
    lock.l_type = (mode == forRead) ? F_RDLCK : F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if(fcntl(handle(), F_SETLK, &lock)) {
	close();
	m_error = QFileDevice::OpenError;
	return false;
    }
    return true;
}

bool XFile::safeRename(const QString &oldName, const QString &newName) {
    int r;
    r = ::rename(oldName.toUtf8().constData(), newName.toUtf8().constData());
    return r == 0;
}

bool XFile::makeInvisible(const QString &fileName, bool invisible) {
    Q_UNUSED(fileName);
    Q_UNUSED(invisible);
    return true;
}

#endif

bool XFile::open(QIODevice::OpenMode flags)
{
    if (flags & QIODevice::ReadOnly)
        return openFor(forRead);
    else
        return openFor(forWrite);
}

QFileDevice::FileError XFile::error() const {
    if(m_error != QFileDevice::NoError)
	return m_error;
    return QFile::error();
}
