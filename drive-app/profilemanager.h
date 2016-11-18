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

#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <QLocalServer>
#include <QObject>
#include <QFuture>
#include <QFutureWatcher>

class MainController;

class ProfileManager : public QObject
{
    Q_OBJECT
public:
    struct ProfileStatus
    {
        ProfileStatus(QString name, QString status, bool autostart) {
            profileName = name;
            profileStatus = status;
            autostartEnabled = autostart;
        }
        QString profileName;
        QString profileStatus;
        bool autostartEnabled;
    };

private:
    explicit ProfileManager(QObject *parent = 0);
    QByteArray m_data;
    QLocalServer *m_localServer;
    QString m_profile;
    MainController* m_mainController;
    QFuture<QList<ProfileStatus>> m_future;
    QFutureWatcher<QList<ProfileStatus>> m_futureWatcher;

public:
    static ProfileManager* instance();
    static QString getLocalServerName(QString profile);
    QStringList listProfiles() const;
    QStringList profileStatus(QString profile);
    bool removeProfile(QString profile);
    void setMainController(MainController* mainController);
    void createProfile(QString name);
    void closeAllProfiles();

private slots:
    void onFutureFinished();

public slots:
    void requestProfilesStatus();

signals:
    void gotProfilesStatus(const QList<ProfileStatus>& status);

};

#endif // PROFILEMANAGER_H
