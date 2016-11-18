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

#ifndef VOLUMESWIDGET_H
#define VOLUMESWIDGET_H

#include "sxprogressbar.h"
#include "sxconfig.h"
#include "getpassworddialog.h"
#include "sxauth.h"
#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <sxvolumeentry.h>

class VolumesWidget : public QScrollArea
{
    Q_OBJECT
public:
    explicit VolumesWidget(QWidget *parent = 0);
    ~VolumesWidget();
    void showMessage(const QString &message);
    void updateVolumes(QString m_vcluster, QList<SxVolumeEntry> &volumes, const QHash<QString, QString>& config, QSet<QString> &mLockedVolumes, QHash<QString, VolumeEncryptionType> &encryptedVolumesTypes);
    void setClusterUuid(const QByteArray &clusterUuid);
    void setAuth(const SxAuth &auth);
    void lockVolume(const QString& volume);
    QHash<QString, QString> selectedVolumes() const;
signals:
    void sig_volumeUnlocked(const QString &volume);

public slots:

private:
    struct VolProgressPtr
    {
        QFrame *frame;
        QCheckBox *label;
        QLineEdit *folder;
        QPushButton *browseDir;
        QPushButton *unlock;
        SxProgressBar *progressBar;
        QSpacerItem *spacer1, *spacer2;
    };

    QGridLayout *mLayout;
    QSpacerItem *mSpacer;
    QLabel *mLabel;
    QMap<QString, VolProgressPtr> m_progressBar;
    QString m_vcluster;
    QByteArray mClusterUuid;
    SxAuth mAuth;
    QWidget *mContent;
};

#endif // VOLUMESWIDGET_H
