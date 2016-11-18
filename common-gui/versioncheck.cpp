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

#include "versioncheck.h"
#include "util.h"
#include "sxlog.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include "downloaddialog.h"
#include <QApplication>
#include <QFuture>
#include <QMessageBox>
#include <QMetaEnum>
#include <QProcess>
#include <QtConcurrent>
#include <QFutureWatcher>

QString VersionCheck::mUpdateScriptName;

VersionCheck::SxVersion::SxVersion(QString version)
{
    static QRegExp versionTest("(\\d+)[.](\\d+)[.](\\d+)([.]beta.(\\d+))?", Qt::CaseInsensitive, QRegExp::RegExp2);
    if (versionTest.exactMatch(version))
        m_version=version;
    else
        m_version="0.0.0";
}

bool VersionCheck::SxVersion::isBeta() const
{
    return m_version.contains(".beta.");
}

QString VersionCheck::SxVersion::toString()
{
    return m_version;
}

bool VersionCheck::SxVersion::operator >(const VersionCheck::SxVersion &other) {
    if (this->isBeta() && other.isBeta())
    {
        int v1 = versionStringToInt(m_version.split(".beta.").first());
        int v2 = versionStringToInt(other.m_version.split(".beta.").first());
        if (v1 > v2)
            return true;
        else if (v1==v2)
        {
            int n1 = m_version.split(".beta.").last().toInt();
            int n2 = other.m_version.split(".beta.").last().toInt();
            return (n1>n2);
        }
        else
            return false;
    }
    else if (this->isBeta())
    {
        int v1 = versionStringToInt(m_version.split(".beta.").first());
        int v2 = versionStringToInt(other.m_version);
        return (v1>v2);
    }
    else if (other.isBeta())
    {
        int v1 = versionStringToInt(m_version);
        int v2 = versionStringToInt(other.m_version.split(".beta.").first());
        return (v1>=v2);
    }
    else
    {
        int v1 = versionStringToInt(m_version);
        int v2 = versionStringToInt(other.m_version);
        return (v1>v2);
    }
}

bool VersionCheck::SxVersion::operator ==(const VersionCheck::SxVersion &other)
{
        return (m_version == other.m_version);
}

VersionCheck::VersionCheck()
{
    mParentWidget = nullptr;
    m_enabled = 0;
    m_netAccMan = 0;
    m_initlalCheck = false;
    m_showResult = false;
    m_downloadFromBetaRepo = false;
    m_timer.setSingleShot(true);
    m_tempfile = 0;
    m_versionFile = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/"+mApplicationName.toLower()+".version";
}

void VersionCheck::initializeVersionCheck(const QString &applicationName, const QString &version, const QString &urlRepoRelease, const QString &urlRepoBeta, const QString &urlTemplateCheck, const QString &urlTemplateDownload, const QString &updateScriptName)
{
    instance()->mApplicationName = applicationName;
    instance()->mVersion = version;
    instance()->mUrlRelease = urlRepoRelease;
    instance()->mUrlBeta = urlRepoBeta;
    instance()->mUrlTemplateCheck = urlTemplateCheck;
    instance()->mUrlTemplateDownload = urlTemplateDownload;
    instance()->mUpdateScriptName = updateScriptName;
    instance()->readVersionFile();
}

VersionCheck *VersionCheck::instance()
{
    static VersionCheck s_versionCheck;
    return &s_versionCheck;
}

const QString VersionCheck::updateScriptName()
{
#if defined Q_OS_WIN
    return mUpdateScriptName+".bat";
#elif defined Q_OS_MAC
    return "./"+mUpdateScriptName+".sh";
#else
    return "echo";
#endif
}

void VersionCheck::replyFinished(QNetworkReply *reply)
{
    m_replies.removeAll(reply);
    reply->deleteLater();

    if (reply->error()) {
        QVariant timeout = reply->property("timeout");
        if (timeout.isValid() && timeout.toBool()) {
            logWarning("Received timeout on request " + reply->request().url().toString());
        }
        else {
            static auto object = QNetworkReply::staticMetaObject;
            static auto index = object.indexOfEnumerator("NetworkError");
            if (index>=0) {
                static auto enumator = object.enumerator(index);
                logWarning(QString("received error %1").arg(enumator.valueToKey(reply->error())));
            }
            else {
                logWarning(QString("received error %1").arg(reply->error()));
            }
        }
    }

    QString requestUrl = reply->request().url().toString();
    if (reply->property("check-version").isValid()) {
        if (!reply->error()) {
            QString versionString = reply->readAll().trimmed();
            if (m_availableVersion.isEmpty()) {
                m_availableVersion = versionString;
                m_downloadFromBetaRepo = requestUrl.contains(mUrlBeta);
            }
            else
            {
                SxVersion v1(m_availableVersion);
                SxVersion v2(versionString);
                if (v2>v1) {
                    m_availableVersion = versionString;
                    m_downloadFromBetaRepo = requestUrl.contains(mUrlBeta);
                }
                else if (v1==v2 && !v1.isBeta()) {
                    if (m_downloadFromBetaRepo && !requestUrl.contains(mUrlBeta))
                        m_downloadFromBetaRepo = false;
                }
            }
        }
        if (m_replies.isEmpty()) {
            m_timer.stop();
            m_timer.disconnect();
            if (m_availableVersion.isEmpty()) {
                logWarning("Version check failed");
                emit versionCheckFailed(m_showResult, "");
                if (m_initlalCheck) {
                    m_initlalCheck = false;
                    emit initialCheckFinished();
                }
                if (m_enabled) {
                    connect(&m_timer, &QTimer::timeout, this, &VersionCheck::checkNow);
                    m_timer.start(retrySec*1000);
                }
            }
            else {
                logVerbose(QString("VersionCheck finished. Available version: %1").arg(m_availableVersion));
                SxVersion v1(m_minVersion);
                SxVersion v2(m_availableVersion);
                if (v2>v1) {
                    if (m_initlalCheck) {
                        downloadUpdate(m_availableVersion, m_downloadFromBetaRepo);
                        return;
                    }
                    else
                        emit newVersionAvailable(m_availableVersion);
                }
                else {
                    if (m_initlalCheck) {
                        m_initlalCheck = false;
                        emit initialCheckFinished();
                    }
                    else {
                        emit noNewVersion(m_showResult);
                    }
                    if (m_enabled)
                    {
                        connect(&m_timer, &QTimer::timeout, this, &VersionCheck::checkNow);
                        m_timer.start(retrySec*1000);
                    }
                }
            }
            m_showResult = false;
            m_netAccMan->deleteLater();
            m_netAccMan = 0;
        }
    }
    else {
        if (reply->error()) {
            logWarning("Downloading update failed "+ reply->errorString());
            return;
        }
        if(!m_tempfile) {
            logError("tempfile is missing");
            return;
        }
        logVerbose("Downloading update finished " + m_availableVersion);
        m_updateVersion = m_availableVersion;
        writeVersionFile();

        QString pwd = QDir::currentPath();
        QDir::setCurrent(QCoreApplication::applicationDirPath());
        m_tempfile->setAutoRemove(false);
        QString filename = QDir::toNativeSeparators(m_tempfile->fileName());
        QString cmd = updateScriptName()+" "+filename;
        logVerbose("executing command: " + cmd);
        auto lambda = [this, filename, pwd]() {
            disconnect(this, &VersionCheck::signal_updateFailed, 0, 0);
            logWarning("Failed to install update from file " + filename);
            m_tempfile->remove();
            delete m_tempfile;
            m_tempfile = 0;
            QDir::setCurrent(pwd);

            QMessageBox msg(QMessageBox::Warning, tr("WARNING"), tr("%1 update failed. Version %2 will be marked as broken, and will not be downloaded again.").arg(mApplicationName).arg(m_updateVersion));
            msg.setParent(mParentWidget);
            m_minVersion = m_updateVersion;
            m_updateVersion = "";
            writeVersionFile();
            m_initlalCheck = false;
            emit initialCheckFinished();
            msg.exec();
        };

#ifdef Q_OS_WIN
        if (!QProcess::startDetached("cmd /C start /MIN "+cmd))
            lambda();
        else
            connect(this, &VersionCheck::signal_updateFailed, lambda);
#else
	QTemporaryFile tmpScript(QStandardPaths::writableLocation(QStandardPaths::TempLocation)+"/XXXXXX.sh");
	if (!tmpScript.open()) {
		lambda();
		return;
	}
	logVerbose("update script:" + tmpScript.fileName());
	tmpScript.setAutoRemove(false);
	QString tmpName = tmpScript.fileName();
	tmpScript.close();
	tmpScript.remove();
	QFile::copy(updateScriptName(), tmpName);
	QString log_file = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/log/update.log";
	if (!QProcess::startDetached(tmpName, {filename, log_file}))
	    lambda();
	else {
	    connect(this, &VersionCheck::signal_updateFailed, lambda);
	}
#endif
    }
}

void VersionCheck::downloadProgress(qint64, qint64)
{
    QNetworkReply* reply = static_cast<QNetworkReply*>(sender());

    if (!reply) {
        logError("critical error");
        return;
    }
    if (!m_tempfile) {
        logError("tempfile is missing");
        return;
    }
    if (!m_tempfile->open()) {
        logError("unable to open tempfile");
    }

    m_tempfile->seek(m_tempfile->size());
    m_timer.stop();

    if (reply->isOpen()) {
        auto data = reply->readAll();
        m_tempfile->write(data);

        if (m_tempfile->flush()) {
            m_tempfile->close();
            m_timer.start(timeoutSec*1000);
        }
        else {
            logWarning("unable to flush tempfile");
            reply->finished();
        }
    }
}

void VersionCheck::onNetworkTimeout()
{
    foreach (auto r, m_replies) {
        r->setProperty("timeout", true);
        r->abort();
    }
}

void VersionCheck::slotUpdateNow()
{
    downloadUpdate(m_availableVersion, m_downloadFromBetaRepo);
}

void VersionCheck::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        m_timer.stop();
        if (m_netAccMan) {
            delete m_netAccMan;
            m_netAccMan = 0;
            m_replies.clear();
        }
    }
}

void VersionCheck::setCheckingBeta(bool enabled)
{
    m_checkingBeta = enabled;
}

bool VersionCheck::enabled() const
{
    return m_enabled;
}

bool VersionCheck::betaCheckingEnabled() const
{
    return m_checkingBeta;
}

void VersionCheck::pause()
{
    if (m_replies.count()==1) {
        auto url = m_replies.first()->request().url().toString();
        auto test = mUrlTemplateDownload.split("-").first();
        if (url.contains(test))
            return;
    }
    m_timer.stop();
    foreach (auto r, m_replies) {
            r->abort();
    }
}

void VersionCheck::resume()
{
    if (m_enabled)
        checkNow();
}

bool VersionCheck::initialCheck()
{
    m_initlalCheck = true;
    return checkNow();
}

void VersionCheck::resetVersionCheck()
{
    m_minVersion = mVersion;
    writeVersionFile();
}

void VersionCheck::updateFailed()
{
    emit signal_updateFailed();
}

bool VersionCheck::updateNow()
{
    if (m_availableVersion.isEmpty())
        return false;
    QTimer::singleShot(0, this, SLOT(slotUpdateNow()));
    return true;
}

void VersionCheck::setParentWidget(QWidget *parent)
{
    mParentWidget = parent;
}

QWidget *VersionCheck::parentWidget() const
{
    return mParentWidget;
}

bool VersionCheck::checkNow()
{
    QString version(mVersion);
    if (version.contains("experimental"))
        return false;
#if defined Q_OS_WIN
    static QString operatingSystem = "windows";
#elif defined Q_OS_MAC
    static QString operatingSystem = "macosx";
#elif defined Q_OS_LINUX
    static QString operatingSystem = "linux";
#else
    static QString operatingSystem = "unknown";
#endif
    m_timer.stop();
    m_timer.disconnect();

    if (!m_replies.isEmpty()) {
        logWarning("unable to check version. Previous check is in progress.");
        return false;
    }
    logVerbose("Checking for new version...");
    if (!m_netAccMan) {
        m_netAccMan = new QNetworkAccessManager();
        connect(m_netAccMan, &QNetworkAccessManager::finished, this, &VersionCheck::replyFinished);
    }
    m_availableVersion.clear();
    QNetworkRequest request(mUrlRelease+mUrlTemplateCheck.arg(mVersion).arg(operatingSystem));
    request.setRawHeader("Cache-control", "max-age=60");
    QNetworkReply *reply = m_netAccMan->get(request);
    if (reply) {
        m_replies.append(reply);
        reply->setProperty("check-version", true);
    }
    if (m_checkingBeta) {
        QNetworkRequest request2(mUrlBeta+mUrlTemplateCheck.split("?").first());
        request2.setRawHeader("Cache-control", "max-age=60");
        QNetworkReply *reply2 = m_netAccMan->get(request2);
        if (reply2)
        {
            m_replies.append(reply2);
            reply2->setProperty("check-version", true);
        }
    }
    if (!m_replies.isEmpty()) {
        connect(&m_timer, &QTimer::timeout, this, &VersionCheck::onNetworkTimeout);
        m_timer.start(timeoutSec*1000);
        return true;
    }
    else {
        logWarning("unable to check version. QNetworkAccessManager failed.");
        return false;
    }
}

void VersionCheck::checkVersionShowResult(bool checkBeta)
{
    m_showResult = true;
    m_checkingBeta = checkBeta;
    checkNow();
}

void VersionCheck::downloadUpdate(QString version, bool downloadFromBetaRepo)
{
    qDebug() << "Starting to download update" << version;
    QString ext;
#if defined Q_OS_WIN
    ext=".msi";
#elif defined Q_OS_MAC
    ext=".dmg";
#endif
    auto const tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempfile = new QTemporaryFile(tempPath+"/XXXXXXXXXX"+ext);

    QString url = (downloadFromBetaRepo?mUrlBeta:mUrlRelease)+mUrlTemplateDownload.arg(version+ext);
    QNetworkRequest request(url);
    if (!m_netAccMan) {
        m_netAccMan = new QNetworkAccessManager();
        connect(m_netAccMan, &QNetworkAccessManager::finished, this, &VersionCheck::replyFinished);
    }
    DownloadDialog *d = new DownloadDialog(mParentWidget);
    _sendDownloadQuerry(request, d, 1);
    d->show();
    d->raise();
    d->activateWindow();
    m_timer.start();
}

void VersionCheck::_sendDownloadQuerry(QNetworkRequest request, DownloadDialog *d, int attempt)
{
    QNetworkReply *reply = m_netAccMan->get(request);
    if (!reply) {
        logWarning("Unable to download update. QNetworkAccessManager failed.");
        m_netAccMan->deleteLater();
        m_netAccMan = 0;
        delete m_tempfile;
        return;
    }

    reply->setProperty("attempt", attempt);
    m_replies.append(reply);

    connect(reply, &QNetworkReply::downloadProgress, this, &VersionCheck::downloadProgress);
    connect(reply, &QNetworkReply::downloadProgress, d, &DownloadDialog::setDownloadProgress);
    connect(&m_timer, &QTimer::timeout, this, &VersionCheck::onNetworkTimeout);

    QMetaObject::Connection *conn = new QMetaObject::Connection();
    auto lambda = [d, this, conn](QNetworkReply *r) {
        if (r->error() == QNetworkReply::TimeoutError || r->error() == QNetworkReply::OperationCanceledError) {
            int attempt = r->property("attempt").toInt();
            if (attempt < 3) {
                m_netAccMan->deleteLater();
                m_netAccMan = new QNetworkAccessManager();
                connect(m_netAccMan, &QNetworkAccessManager::finished, this, &VersionCheck::replyFinished);
                QNetworkRequest request = r->request();
                if (!request.hasRawHeader("Cache-control"))
                    request.setRawHeader("Cache-control", "max-age=60");
                _sendDownloadQuerry(request, d, attempt+1);
                return;
            }
        }
        disconnect(*conn);
        delete conn;
        d->close();

        if (r->error()) {
            QString errorMessage = r->errorString();
            if (r->error() == QNetworkReply::OperationCanceledError) {
                errorMessage = QApplication::translate("SXCluster", "Connection to the remote server timed out.");
            }
            QMessageBox::critical(mParentWidget, tr("WARNING"), tr("%1 was unable to download the update.").arg(mApplicationName)+"\n"+errorMessage);
            if (m_initlalCheck) {
                emit initialCheckFinished();
                m_initlalCheck = false;
            }
        }
    };
    *conn = connect(m_netAccMan, &QNetworkAccessManager::finished, lambda);
}

void VersionCheck::writeVersionFile() const
{
    QFile f(m_versionFile);
    if (!f.open(QIODevice::WriteOnly))
        logWarning("unable to create version file "+ m_versionFile);
    else {
        QDataStream ds(&f);
        ds << m_minVersion << m_updateVersion;
        f.close();
    }
}

void VersionCheck::readVersionFile()
{
    QFile f(m_versionFile);
    if (!f.exists()) {
        m_minVersion = mVersion;
        writeVersionFile();
        return;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        m_minVersion = mVersion;
        logWarning("unable to open version file " + m_versionFile);
        return;
    }
    QDataStream ds(&f);
    ds >> m_minVersion >> m_updateVersion;
    f.close();

    bool needWrite = false;
    SxVersion currVer(mVersion);
    SxVersion minVer(m_minVersion);

    if (currVer > minVer) {
        needWrite = true;
        m_minVersion = currVer.toString();
        if (m_updateVersion.isEmpty())
            QTimer::singleShot(0, this, SIGNAL(updateSuccessful()));
    }
    if (!m_updateVersion.isEmpty()) {
        needWrite = true;
        if (m_updateVersion != mVersion) {
            m_minVersion = m_updateVersion;
            logWarning(QString("%1 update failed. Version %2 will be marked as broken, and will not be downloaded again.")
                       .arg(mApplicationName).arg(m_updateVersion).toStdString().c_str());
            QMessageBox msg(QMessageBox::Warning, tr("WARNING"),
                            tr("%1 update failed. Version %2 will be marked as broken, and will not be downloaded again.")
                            .arg(mApplicationName).arg(m_updateVersion));
            msg.setParent(mParentWidget);
            msg.exec();
        }
        else
            QTimer::singleShot(0, this, SIGNAL(updateSuccessful()));
        m_updateVersion = "";
    }
    if (needWrite)
        writeVersionFile();
}
