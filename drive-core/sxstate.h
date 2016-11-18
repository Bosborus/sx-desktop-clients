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

#ifndef SXSTATE_H
#define SXSTATE_H

#include <QDateTime>
#include <QObject>
#include <QSet>

enum class SxStatus {
    idle,
    working,
    paused,
    inactive
};

class SxWarning {
public:
    SxWarning(const QString &volume, const QString &file, const QString &message, bool critical);
    bool isEqual(const QString &volume, const QString &file) const;
    QDateTime eventDate() const;
    QString message() const;
private:
    QString mVolume;
    QString mFile;
    QString mMessage;
    bool mCritical;
    QDateTime mEventDate;
};

class SxState : public QObject
{
    Q_OBJECT
public:
    SxState(QObject *parent = 0);
    SxStatus status() const;
    int warningsCount() const;
    const QList<SxWarning>& warnings() const;
    QString toString() const;

public slots:
    void addWarning(const QString &volume, const QString &file, const QString &message, bool critical);
    void removeWarning(const QString &volume, const QString &file);
    void setStatus(SxStatus status);
    void clearWarnings();

signals:
    void sig_removeRow(int index);
    void sig_appendRow(const QDateTime &eventDate, const QString &message);
    void sig_clear();

private:
    SxStatus mStatus;
    QList<SxWarning> mWarnings;
    QHash<QString, QString> mWarningsSet;
};

Q_DECLARE_METATYPE(SxStatus)

#endif // SXSTATE_H
