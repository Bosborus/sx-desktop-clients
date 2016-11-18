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

#include "volumeswidget.h"
#include "whitelabel.h"
#include "util.h"
#include "getpassworddialog.h"
#include <QDir>
#include <QMessageBox>
#include <QSet>
#include <QDebug>
#include <QFileDialog>
#include <QStandardPaths>

VolumesWidget::VolumesWidget(QWidget *parent) : QScrollArea(parent)
{
    mContent = new QWidget(this);
    mContent->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    setLayout(new QVBoxLayout(this));
    setWidget(mContent);
    setWidgetResizable(true);

    mLayout = new QGridLayout(mContent);
    mLayout->setColumnStretch(2, 1);
    mLayout->setSizeConstraint(QLayout::SetMinimumSize);
    mContent->setLayout(mLayout);

    mLabel = new QLabel(tr("Loading volumes..."), this);
    mLayout->addWidget(mLabel, 0, 0, 1, 5, Qt::AlignHCenter);
    mSpacer = new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
    mLayout->addItem(mSpacer, 1, 0);
}

VolumesWidget::~VolumesWidget() {
}

void VolumesWidget::showMessage(const QString &message)
{
    mLabel->setVisible(true);
    mLabel->setText(message);
}

void VolumesWidget::updateVolumes(QString vcluster, QList<SxVolumeEntry> &volumes, const QHash<QString, QString> &config, QSet<QString> &mLockedVolumes, QHash<QString, VolumeEncryptionType> &encryptedVolumesTypes)
{
    m_vcluster = vcluster;
    mLabel->setVisible(false);
    QSet<QString> reportedVolumes;
    mLayout->removeItem(mSpacer);

    int lastRow = 0;
    for (int i=0; i<mLayout->count(); i++) {
        int row, unhused;
        mLayout->getItemPosition(i, &row, &unhused, &unhused, &unhused);
        if (row>=lastRow)
            lastRow = row+1;
    }

    for (auto vol: volumes)
    {
        QString volname = vol.name();
        reportedVolumes.insert(volname);
        if (!vcluster.isEmpty() && volname.startsWith(vcluster+"."))
            volname = volname.mid(vcluster.size()+1);
        QString volume = vol.name();
        SxProgressBar *bar = findChild<SxProgressBar*>(vol.name());
        if (bar == nullptr)
        {
            VolProgressPtr volData;
            volData.frame = new QFrame(this);
            volData.frame->setFrameStyle(QFrame::StyledPanel);
            volData.frame->setLineWidth(1);
            volData.frame->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

	    volData.spacer1 = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Fixed);
	    volData.spacer2 = new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::Fixed);

	    volData.label = new QCheckBox(volname, this);
	    volData.label->setDisabled(vol.unsupportedFilter());
            volData.label->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
	    bar = volData.progressBar = new SxProgressBar(this);

            bar->setAttribute(Qt::WA_LayoutOnEntireRect);
            bar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
            bar->setObjectName(vol.name());
            bar->setMinimum(0);
            bar->setStyleSheet("text-align: center;");
            volData.folder = new QLineEdit(this);
            volData.folder->setReadOnly(true);
            volData.folder->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

            volData.browseDir = new QPushButton("...", this);
            volData.browseDir->setMaximumWidth(30);
            volData.browseDir->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
#ifdef Q_OS_MAC
            volData.browseDir->setFixedHeight(25);
#endif

            volData.unlock = new QPushButton(tr("unlock"), this);

            connect(volData.unlock, &QPushButton::clicked, [this, volData, volume, encryptedVolumesTypes]() {
                    GetPasswordDialog passwordDialog(mAuth, mClusterUuid, volume, encryptedVolumesTypes.value(volume), this);
                    if (passwordDialog.exec()) {
                        volData.unlock->setVisible(false);
                        emit sig_volumeUnlocked(volume);
                    }
            });

            if (!mLockedVolumes.contains(vol.name()))
                volData.unlock->setVisible(false);

            mLayout->addWidget(volData.frame,          lastRow,    0, 4, 5, 0);
            mLayout->addItem(volData.spacer1,          lastRow,    0, 1, 1, 0);
            mLayout->addItem(volData.spacer2,          lastRow+3,  4, 1, 1, 0);
            mLayout->addWidget(volData.label,          lastRow+1,  1);
            mLayout->addWidget(volData.progressBar,    lastRow+1,  2, 1, 2, 0);
            mLayout->addWidget(volData.unlock,         lastRow+2,  1);
            mLayout->addWidget(volData.folder,         lastRow+2,  2, 1, 1, Qt::AlignTop);
            mLayout->addWidget(volData.browseDir,      lastRow+2,  3, 1, 1, Qt::AlignTop);

            if (config.contains(vol.name())) {
                volData.label->setCheckState(Qt::Checked);
                volData.folder->setText(config.value(vol.name()));
            }
            else {
                volData.folder->setVisible(false);
                volData.browseDir->setVisible(false);
                volData.unlock->setVisible(false);
            }
            auto funcTestDir = [this, config](const QString &directory, const QString &volume) -> bool {
                QFileInfo dir(directory);
                if (!dir.isDir()) {
                    QMessageBox::warning(this, __applicationName, tr("%1 is not a directory.").arg(directory));
                    return false;
                }
                else if (!dir.isReadable()) {
                    QMessageBox::warning(this, __applicationName, tr("%1 directory is not readable.").arg(directory));
                    return false;
                }
                else if (!dir.isWritable()) {
                    QMessageBox::warning(this, __applicationName, tr("%1 directory is not writable.").arg(directory));
                    return false;
                }
                foreach (QString volume, config.keys()) {
                    QDir d1(directory);
                    QDir d2(config.value(volume));
                    if (d1.absolutePath() == d2.absolutePath()) {
                        QMessageBox::warning(this, __applicationName, tr("directory is already used by volume %1").arg(volume));
                        return false;
                    }
                    if (isSubdirectory(d1, d2)) {
                        QMessageBox::warning(this, __applicationName, tr("selected directory contains other volumes"));
                        return false;
                    }
                    else if (isSubdirectory(d2, d1)) {
                        QMessageBox::warning(this, __applicationName, tr("cannot choose directory inside other volume"));
                        return false;
                    }
                }
                foreach (QString vol, m_progressBar.keys()) {
                    if (vol == volume)
                        continue;
                    if (!m_progressBar.value(vol).label->isChecked())
                        continue;
                    QDir d1(directory);
                    QDir d2(m_progressBar.value(vol).folder->text());
                    if (d1.absolutePath() == d2.absolutePath()) {
                        QMessageBox::warning(this, __applicationName, tr("directory is already used by volume %1").arg(volume));
                        return false;
                    }
                    if (isSubdirectory(d1, d2)) {
                        QMessageBox::warning(this, __applicationName, tr("selected directory contains other volumes"));
                        return false;
                    }
                    else if (isSubdirectory(d2, d1)) {
                        QMessageBox::warning(this, __applicationName, tr("cannot choose directory inside other volume"));
                        return false;
                    }
                }
                return true;
            };

            connect(volData.browseDir, &QPushButton::clicked, [this, volData, funcTestDir, volume, config]() {
                QFileDialog dialog;
                dialog.setFileMode(QFileDialog::Directory);
                dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
                if (dialog.exec()) {
                    QString directory = dialog.selectedFiles().first();

                    if (config.contains(volume)) {
                        QDir d1(directory);
                        QDir d2(config.value(volume));
                        if (d1.absolutePath() == d2.absolutePath())
                            return;
                    }
                    if (funcTestDir(directory, volume))
                        volData.folder->setText(directory);
                }
            });

            connect(volData.label, &QCheckBox::clicked, [volData, this, volume, funcTestDir, config, encryptedVolumesTypes, mLockedVolumes](bool checked) {
                if (checked) {
                    if (config.contains(volume)) {
                        volData.folder->setText(config.value(volume));
                    }
                    else {
                        bool failed = false;
                        QFileDialog dialog;
                        dialog.setFileMode(QFileDialog::Directory);
                        dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
                        if (dialog.exec()) {
                            QString directory = dialog.selectedFiles().first();
                            if (funcTestDir(directory, volume)) {
                                volData.folder->setText(directory);
                                if (encryptedVolumesTypes.contains(volume)) {
                                    GetPasswordDialog passwordDialog(mAuth, mClusterUuid, volume, encryptedVolumesTypes.value(volume), this);
                                    if (!passwordDialog.exec())
                                        failed = true;
                                }
                            }
                            else
                                failed = true;
                        }
                        else
                            failed = true;
                        if (failed) {
                            volData.label->setChecked(false);
                            return ;
                        }
                    }
                }
                volData.folder->setVisible(checked);
                volData.browseDir->setVisible(checked);
                volData.unlock->setVisible(checked && mLockedVolumes.contains(volume));
            });
            lastRow+=4;
            m_progressBar.insert(vol.name(), volData);
        }
        auto volUsd = formatSize(vol.usedSize());
        auto volSz = formatSize(vol.size());
        auto spaceLabel = QString("%p% (%1 / %2)").arg(volUsd).arg(volSz);
        if (bar)
        {
            bar->setFormat(spaceLabel);
            bar->setMaximum(100);
            if(vol.size())
                bar->setValue(static_cast<int>(100.0*vol.usedSize()/vol.size()));
            bar->show();
        }
        else
        {
            qWarning() << "Something went wrong, no progress bar widget for volume" << volname;
        }
    }
    if (!volumes.isEmpty())
        mLayout->addItem(mSpacer, lastRow, 0);

    // remove progress bar for volumes which are not reported anymore
    for (auto it = m_progressBar.begin(); it != m_progressBar.end();)
    {
        if (!reportedVolumes.contains(it.key()))
        {
            const VolProgressPtr volData = it.value();
            volData.browseDir->deleteLater();
            volData.folder->deleteLater();
            volData.frame->deleteLater();
            volData.label->deleteLater();
            volData.progressBar->deleteLater();
            mLayout->removeItem(volData.spacer1);
            mLayout->removeItem(volData.spacer2);
            delete volData.spacer1;
            delete volData.spacer2;
            it = m_progressBar.erase(it);
        }
        else
        {
            ++it;
        }
    } //*/
}

void VolumesWidget::setClusterUuid(const QByteArray &clusterUuid)
{
    mClusterUuid = clusterUuid;
}

void VolumesWidget::setAuth(const SxAuth &auth)
{
    mAuth = auth;
}

void VolumesWidget::lockVolume(const QString &volume)
{
    if (!m_progressBar.contains(volume))
        return;
    auto volData = m_progressBar.value(volume);
    if (!volData.label->isChecked())
        return;
    volData.unlock->setVisible(true);
}

QHash<QString, QString> VolumesWidget::selectedVolumes() const
{
    QHash<QString, QString> result;
    foreach (QString volume, m_progressBar.keys()) {
        auto volData = m_progressBar.value(volume);
        if (volData.label->isChecked()) {
            result.insert(volume, volData.folder->text());
        }
    }
    return result;
}
