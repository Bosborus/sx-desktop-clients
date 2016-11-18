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

#include "sxcluster.h"
#include "sxquery.h"
#include "sxqueryresult.h"
#include "sxfilter.h"

#include <memory>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDebug>
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QSslCertificate>
#include <QPair>
#include <QTemporaryFile>
#include <QDir>
#include <QUrlQuery>
#include <QUuid>
#include <QNetworkInterface>
#include <QCoreApplication>
#include "xfile.h"
#include "sxlog.h"
#include "volumeconfigwatcher.h"

#ifdef Q_OS_LINUX
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif
    #include <fcntl.h>
#endif

QString SxCluster::sClientVersion = "unknown";

static const QHash<QNetworkReply::NetworkError, QString> sNetworkError = {
    {QNetworkReply::ConnectionRefusedError,             QT_TRANSLATE_NOOP("SxErrorMessage","Remote server refused the connection.")},
    {QNetworkReply::RemoteHostClosedError,              QT_TRANSLATE_NOOP("SxErrorMessage","Remote server closed connection prematurely, before the entire reply was received and processed.")},
    {QNetworkReply::HostNotFoundError,                  QT_TRANSLATE_NOOP("SxErrorMessage","Remote host name was not found.")},
    {QNetworkReply::TemporaryNetworkFailureError,       QT_TRANSLATE_NOOP("SxErrorMessage","Connection was broken due to disconnection from the network, however the system has initiated roaming to another access point. The request should be resubmitted and will be processed as soon as the connection is re-established.")},
    {QNetworkReply::NetworkSessionFailedError,          QT_TRANSLATE_NOOP("SxErrorMessage","Connection was broken due to disconnection from the network or failure to start the network.")},
    {QNetworkReply::ContentReSendError,                 QT_TRANSLATE_NOOP("SxErrorMessage","Remote server could not read uploaded data, request need to be sent again.")},
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
    {QNetworkReply::InternalServerError,                QT_TRANSLATE_NOOP("SxErrorMessage","Remote server encountered an unexpected condition which prevented it from fulfilling the request.")},
    {QNetworkReply::ServiceUnavailableError,            QT_TRANSLATE_NOOP("SxErrorMessage","Remote server is unable to handle the request at this time, service is unavailable.")},
    {QNetworkReply::UnknownServerError,                 QT_TRANSLATE_NOOP("SxErrorMessage","An unknown error related to the server response was detected.")},
    #endif
    {QNetworkReply::UnknownNetworkError,                QT_TRANSLATE_NOOP("SxErrorMessage","An unknown network-related error was detected.")},
    {QNetworkReply::BackgroundRequestNotAllowedError,   QT_TRANSLATE_NOOP("SxErrorMessage","The background request is not currently allowed due to platform policy.")},
    {QNetworkReply::ProxyConnectionRefusedError,        QT_TRANSLATE_NOOP("SxErrorMessage","Connection to the proxy server was refused.")},
    {QNetworkReply::ProxyConnectionClosedError,         QT_TRANSLATE_NOOP("SxErrorMessage","Proxy server closed the connection prematurely, before the entire reply was received and processed.")},
    {QNetworkReply::ProxyNotFoundError,                 QT_TRANSLATE_NOOP("SxErrorMessage","Proxy host name was not found.")},
    {QNetworkReply::ProxyTimeoutError,                  QT_TRANSLATE_NOOP("SxErrorMessage","Connection to the proxy timed out or the proxy did not reply in time to the request sent.")},
    {QNetworkReply::ProxyAuthenticationRequiredError,   QT_TRANSLATE_NOOP("SxErrorMessage","Proxy requires authentication in order to honour the request but did not accept any credentials offered (if any).")},
    {QNetworkReply::ProtocolUnknownError,               QT_TRANSLATE_NOOP("SxErrorMessage","Network Access API cannot honor the request because the protocol is not known.")}
};

QString retriveClusterUuid(QNetworkReply *reply) {
    if(reply->hasRawHeader("SX-Cluster")) {
    // Match SX-Cluster header such as "0.5.beta4-83-g26c0ed2 (ccc408c8-ef59-4e58-a34e-0ba1c6d9a7c1) ssl"
        QRegExp srvre("(\\d+)[.](\\d+)(?:[.-].*)? \\((.*)\\)( ssl)?", Qt::CaseInsensitive, QRegExp::RegExp2);
        if(srvre.exactMatch(reply->rawHeader("SX-Cluster"))) {
            return srvre.cap(3);
        }
    }
    return QString();
}

SxQueryResult * processReply(QNetworkReply* reply, QString clusterUuid) {
    logEntry("");
    SxErrorCode error = SxErrorCode::NoError;
    QString errorMessage;
    QString errorMessageTr;

    QString etag;
    bool viaSxCache = reply->hasRawHeader("via");
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray body;

    if (reply->isOpen())
        body = reply->readAll();

    if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
        error = SxErrorCode::SslError;
        if (reply->property("lastSslError").isValid())
            errorMessage = reply->property("lastSslError").toString();
        else
            errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "SSL Handshake failed.");
        goto endFunction;
    }
    if (sNetworkError.contains(reply->error())) {
        error = SxErrorCode::NetworkError;
        errorMessage = sNetworkError.value(reply->error());
        goto endFunction;
    }
    if (reply->error() == QNetworkReply::TimeoutError || reply->error() == QNetworkReply::OperationCanceledError) {
        error = SxErrorCode::Timeout;
        errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage","Connection to the remote server timed out.");
        goto endFunction;
    }
    {
        QString srvid = retriveClusterUuid(reply);
        if (srvid.isEmpty()) {
            error = SxErrorCode::InvalidServer;
            errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Invalid server version");
            goto endFunction;
        }
        if (srvid != clusterUuid) {
            error = SxErrorCode::InvalidServer;
            auto messageTemplate = QT_TRANSLATE_NOOP("SxErrorMessage", "Invalid server (expected %1 but %2 received)");
            errorMessage = QString(messageTemplate).arg(clusterUuid).arg(srvid);
            errorMessageTr = QCoreApplication::translate("SxErrorMessage",  messageTemplate).arg(clusterUuid).arg(srvid);
            goto endFunction;
        }
    }

    if(httpStatus == 200) {
        if (reply->hasRawHeader("ETag"))
            etag = reply->rawHeader("ETag");
        goto endFunction;
    }

    if (httpStatus == 304) {
        error = SxErrorCode::NotChanged;
        errorMessage = "Nothing changes";
        goto endFunction;
    }
    if (httpStatus == 429) {
        error = SxErrorCode::TooManyRequests;
        goto endFunction;
    }

    {
        QJsonParseError jsonErr;
        QJsonDocument json = QJsonDocument::fromJson(body, &jsonErr);
        if(jsonErr.error == QJsonParseError::NoError &&
           json.isObject() &&
           json.object().contains("ErrorMessage") &&
           json.object().value("ErrorMessage").isString())
        {
            QString msg = json.object().value("ErrorMessage").toString();
            if (msg=="Invalid credentials") {
                error = SxErrorCode::InvalidCredentials;
                errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Invalid credentials");
            }
            else if (msg == "Not Found") {
                error = SxErrorCode::NotFound;
                errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Not Found");
            }
            else {
                error = SxErrorCode::UnknownError;
                errorMessage = msg;
            }
        }
        else {
            error = SxErrorCode::UnknownError;
            errorMessage = "Server error: " + QString::number(httpStatus);
        }
        if (httpStatus == 500)
            error = SxErrorCode::CriticalServerError;
    }
    endFunction:
    if (errorMessageTr.isEmpty())
        errorMessageTr = QCoreApplication::translate("SxErrorMessage",  errorMessage.toUtf8().constData());
    return new SxQueryResult(reply->request().url().host(), httpStatus, SxError(error, errorMessage, errorMessageTr) ,body, viaSxCache, etag);

}

bool isCertFor(const QSslCertificate &cert, const QString &subject, QString *errorMessage) {
    logEntry("");
    /* Using alternative names */
    QStringList altNames = cert.subjectAlternativeNames().values(QSsl::DnsEntry);
    if(!altNames.isEmpty()) {
        QStringList subjLabels = subject.split('.');
        int numLabels = subjLabels.size();
        if(!numLabels)
            return false;
        for(int i=0; i<altNames.size(); i++) {
            QString altName = altNames.at(i);
            int j;
            for(j=0; j<altName.size(); j++)
                if(altName.at(j).unicode() == 0 || altName.at(j).unicode() > 127)
                    break;
            if(j<altName.size())
                continue; /* Skip IDNs and NULs */

            QStringList altLabels = altName.split('.');
            if(numLabels != altLabels.size())
                continue;

            if(numLabels > 2 && /* No wildcards for *.com */
                    !altName.contains(".xn--", Qt::CaseInsensitive) && /* No wildcards for punycoded IDNs */
                    altLabels.first().contains('*')) {
                QRegExp wildRe(QRegExp::escape(altLabels.first()).replace("\\*", ".*"), Qt::CaseInsensitive, QRegExp::RegExp2);
                if(!wildRe.exactMatch(subjLabels.first()))
                    continue;
                j = 1;
            } else
                j = 0;
            for(; j<numLabels; j++) {
                if(subjLabels.at(j).compare(altLabels.at(j), Qt::CaseInsensitive))
                    break;
            }
            if(j == numLabels)
                return true;
        }
        if (errorMessage) {
            *errorMessage= QObject::tr("Certificate name mismatch: cluster name \"%1\" doesn't match any in %2")
                    .arg(subject).arg("(\""+ altNames.join(", ") + "\")");
        }
        logWarning("Certificate name mismatch: cluster name" + subject + "doesn't match any in (" + altNames.join(",")+")");
        return false;
    }

    /* No alternative names found, using CN */
    QStringList peerCNs = cert.subjectInfo(QSslCertificate::CommonName);
    if(peerCNs.size() != 1)
        return false;
    if(peerCNs.last() != subject) {
        logWarning("Certificate name mismatch: cluster name is " +subject +" but peer certificate is for " + peerCNs.last());
        return false;
    }
    return true;
}

SxCluster::SxCluster(const SxAuth& auth, QByteArray uuid, const QStringList& nodes) : mSxAuth(auth)
{
    mTimeoutMultiplier = 5;
    mTimeoutProgress = sTimeoutProgress * 1000 * mTimeoutMultiplier;
    mTimeoutInitial = sTimeoutInitial * 1000 * mTimeoutMultiplier;
    mInitialReplyTimes.append(sTimeoutInitial * 1000);
    mProgressReplyTimes.append(sTimeoutProgress * 1000);
    mInitialReplyTimeMax = mInitialReplyTimeCount = mProgressReplyTimeMax = mProgressReplyTimeCount = 0;

    mClusterUuid = uuid;
    mNodeList = nodes;
    mNetworkAccessManager = new QNetworkAccessManager(this);
    mTimeDrift = 0;
    mViaSxCache = false;
    mCallbackConfirmCert = nullptr;
    mCallbackGetInput = nullptr;
    mUseApplianceNodeList = false;
}

SxCluster::~SxCluster()
{
    foreach (auto vol, mVolumeList) {
        delete vol;
    }
    foreach (auto netMan, mNetworkManagersToRemove) {
        netMan->deleteLater();
    }
    mNetworkAccessManager->deleteLater();
}

void SxCluster::setClientVersion(const QString &version)
{
    sClientVersion = version;
}

QString SxCluster::getClientVersion()
{
    return sClientVersion;
}

SxCluster *SxCluster::initializeCluster(const SxAuth &auth, QByteArray uuid, std::function<bool(QSslCertificate &, bool)> checkSslCallback, QString &errorMessage)
{
    logEntry("");
    errorMessage.clear();
    if (!auth.isValid()) {
        errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Configuration is not valid");
        logWarning(errorMessage);
        return nullptr;
    }
    QStringList nodes;
    if (auth.initialAddress().isEmpty()) {
        QHostInfo hostInfo = QHostInfo::fromName(auth.clusterName());
        foreach (QHostAddress address, hostInfo.addresses()) {
            nodes << address.toString();
        }
    }
    else {
        nodes << auth.initialAddress();
    }
    if (nodes.isEmpty()) {
        errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Unable to locate cluster nodes");
        logWarning(errorMessage);
        return nullptr;
    }
    SxCluster *c = new SxCluster(auth, uuid, nodes);
    c->mNetworkConfiguration = QNetworkInterface::allAddresses();
    c->setCheckSslCallback(checkSslCallback);
    if (!c->_listNodes(c->mNodeList))
        goto onError;
    if(!c->_getClusterMetadata(c->mMeta))
        logWarning("GetClusterMeta failed: " + c->lastError().errorMessage());
    if (!c->_getUserDetails(c->mUserInfo))
        goto onError;

    if (c->mMeta.contains("appliance_ip_list")) {
        auto value = QString::fromUtf8(c->mMeta.value("appliance_ip_list"));
        logInfo("appliance_ip_list: "+value);
        QStringList ip_list = value.split(";", QString::SkipEmptyParts);
        if (!ip_list.isEmpty()) {
            bool usePrivateIp = true;
            foreach (QString ip, ip_list) {
                logInfo("testing "+ip);
                QString uuid, errorMessage;
                if (!getClusterUUID(auth.clusterName(), ip, auth.use_ssl(), auth.port(), uuid, errorMessage, 5)) {
                    logInfo("node "+ip+" unreachable");
                    usePrivateIp = false;
                    break;
                }
                if (uuid != c->uuid()) {
                    logInfo("node "+ip+": cluster UUID mismatch");
                    usePrivateIp = false;
                    break;
                }
            }
            if (usePrivateIp) {
                c->mNodeList = ip_list;
                c->mUseApplianceNodeList = true;
            }
        }
    }

    logVerbose("node list: "+c->mNodeList.join(", "));
    return c;

    onError:
    errorMessage = c->mLastError.errorMessage();
    logWarning(c->mLastError.errorMessage());
    delete c;
    return nullptr;
}

bool SxCluster::getClusterUUID(const QString &cluster, const QString &initialAddress, const bool &ssl, const int &port, QString &uuid, QString &errorMessage, int timeout)
{
    uuid.clear();
    QStringList nodes;
    if (initialAddress.isEmpty()) {
        QHostInfo hostInfo = QHostInfo::fromName(cluster);
        foreach (QHostAddress address, hostInfo.addresses()) {
            nodes << address.toString();
        }
    }
    else {
        nodes << initialAddress;
    }
    if (nodes.isEmpty()) {
        errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Unable to locate cluster nodes");
        return false;
    }
    while (!nodes.isEmpty()) {
        QString urlString = (ssl ? "https://" : "http://") + nodes.takeFirst();
        if ((ssl && port != 443) || (!ssl && port != 80)) {
            urlString += ":"+QString::number(port);
        }
        urlString += "/?nodeList";
        QUrl url(urlString);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "sxqt-"+SxCluster::getClientVersion());
        req.setRawHeader("SX-Cluster-Name", cluster.toUtf8());
        QNetworkAccessManager man;
        connect(&man, &QNetworkAccessManager::sslErrors, [](QNetworkReply* r, const QList<QSslError>&) {
            r->ignoreSslErrors();
        });
        QNetworkReply *reply = man.get(req);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, [&loop, reply, &uuid, &errorMessage]() {
            if (reply->rawHeader("Server").startsWith("libres3")) {
                errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "You must connect directly to SX, not LibreS3.");
                loop.quit();
                return;
            }
            uuid = retriveClusterUuid(reply);
            if (reply->error() != QNetworkReply::NoError)
                errorMessage = reply->errorString();
            else {
                if (uuid.isEmpty()) {
                    errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Unable to find cluster UUID - not a valid SX node.");
                }
                else {
                    errorMessage.clear();
                }
            }
            loop.quit();
        });
        if (timeout < 0)
            QTimer::singleShot(sTimeoutInitial*1000, reply, SLOT(abort()));
        else
            QTimer::singleShot(timeout*1000, reply, SLOT(abort()));
        loop.exec();
        if (!uuid.isEmpty())
            break;
    }
    return !uuid.isEmpty();
}

bool SxCluster::getEnterpriseAuth(const QString &server, const QString &user, const QString &pass, const QString &devname,
                                  std::function<bool(const QSslCertificate&, bool)> checkCert,
                                  SxUrl &sx_url, QString &errorMessage)
{
    errorMessage.clear();
    QNetworkAccessManager nam;
#ifndef Q_OS_MAC
    if(nam.networkAccessible() != QNetworkAccessManager::Accessible) {
        logWarning("The network is currently down or not accessible.");
        errorMessage = tr("The network is currently down or not accessible.");
        return false;
    }
#endif

    QUrl postUrl = QUrl("https://"+ server + "/.auth/api/v1/create");
    postUrl.setUserName(user, QUrl::DecodedMode);
    postUrl.setPassword(pass, QUrl::DecodedMode);

    static QRegExp macre("^([0-9a-f]{2})(:[0-9a-f]{2}){5}", Qt::CaseInsensitive, QRegExp::RegExp2);
    QString m_uniq;
    QString host = QHostInfo::localHostName();
    QList<QNetworkInterface> nifs = QNetworkInterface::allInterfaces();
    while(!nifs.empty()) {
        QNetworkInterface nif = nifs.takeFirst();
        QString hwaddr = nif.hardwareAddress();
        if(nif.flags().testFlag(QNetworkInterface::IsLoopBack) ||
           macre.indexIn(hwaddr) != 0)
            continue;
        QString msb = macre.cap(1).toLower();
        if(msb == "00" || msb == "ff" || msb.toInt(0, 16) & 2)
            continue; // Filter out non unique mac addies
        m_uniq = hwaddr;
        logVerbose(QString("m_uniq: Using hw address of interface %1 %2").arg(nif.name()).arg(m_uniq));
        break;
    }
    if(m_uniq.isEmpty()) {
        if(!host.isEmpty())
            m_uniq = host;
        else
            m_uniq = QUuid::createUuid().toString();
        logVerbose(QString("m_uniq: Falling back to %1").arg(m_uniq));
    }

    QUrlQuery postQuery;
    postQuery.addQueryItem("display", devname);
    postQuery.addQueryItem("unique", m_uniq);

    QNetworkRequest req(postUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QString("application/x-www-form-urlencoded"));
    QNetworkReply *reply = nam.post(req, postQuery.query(QUrl::FullyEncoded).toUtf8());
    if(!reply) {
        logWarning("The network request failed.");
        errorMessage = tr("The network request failed.");
        return false;
    }
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(sTimeoutInitial*1000);
    QSslCertificate m_acceptedCert;
    connect(reply, &QNetworkReply::sslErrors, [reply, &timer, &errorMessage, &m_acceptedCert, checkCert](const QList<QSslError> &errors) {
        for(int i=0; i<errors.size(); i++) {
            if(errors.at(i).error() == QSslError::SelfSignedCertificate ||
               errors.at(i).error() == QSslError::SelfSignedCertificateInChain) {
                const QSslCertificate crt = errors.at(i).certificate();
                if(crt != m_acceptedCert) {
                    timer.stop();
                    if (checkCert(crt, false))
                        m_acceptedCert = crt;
                    else
                        return;
                }
                continue;
            } else
                return;
        }
        reply->ignoreSslErrors();
        timer.start();
    });

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer.start();
    loop.exec();

    if(reply->error() != QNetworkReply::NoError ||
       reply->attribute(QNetworkRequest::HttpStatusCodeAttribute) != 302 ||
       !reply->hasRawHeader("Location")) {

        if(reply->error() == QNetworkReply::AuthenticationRequiredError)
            errorMessage = tr("The enterprise credentials provided appear to be invalid.\nPlease make sure the data you have entered are correct.");
        else if(reply->error() != QNetworkReply::SslHandshakeFailedError)
            errorMessage = tr("The provided login server appears to be invalid.")+QString(" (%1)").arg(server);
        else
            errorMessage = reply->errorString();

        logWarning(errorMessage);
        logDebug(reply->readAll());
        reply->deleteLater();
        return false;
    }

    QByteArray raw_data = reply->rawHeader("Location");
    QString raw_url = QUrl::fromPercentEncoding(raw_data);
    auto url = SxUrl(raw_url);
    if (!url.isValid()) {
        errorMessage = tr("");
        return false;
    }
    sx_url = url;
    return true;
}

void SxCluster::setCheckSslCallback(std::function<bool (QSslCertificate &, bool)> checkSslCallback)
{
    logEntry("");
    mCallbackConfirmCert = checkSslCallback;
}

SxQueryResult *SxCluster::sendQuery(SxQuery *query, QStringList targetList, const QString& etag)
{
    logEntry("");
    mLastError = SxError();
    QHash<SxQuery*, QStringList*> queries;
    queries.insert(query, &targetList);
    QPair<SxQuery *, SxQueryResult *> result = querySelect(queries, etag);
    return result.second;
}

QNetworkReply * SxCluster::sendNetworkRequest(SxQuery* query, QNetworkRequest req, bool seccondAttempt, int delay)
{
    logEntry("");
    if (seccondAttempt || delay) {
        QTimer timer;
        timer.setTimerType(Qt::PreciseTimer);
        timer.setSingleShot(true);
        QEventLoop loop;
        connect(&timer, &QTimer::timeout, [&loop]() { loop.exit(); });
        timer.start(seccondAttempt ? 1000 : delay);
        loop.exec();
    }

    QNetworkReply *reply;
    switch (query->queryType()) {
    case SxQuery::GET:
       reply = mNetworkAccessManager->get(req);
       break;
    case SxQuery::PUT:
    case SxQuery::JOB_PUT:
       reply = mNetworkAccessManager->put(req, query->body());
       break;
    case SxQuery::DELETE:
    case SxQuery::JOB_DELETE:
       reply = mNetworkAccessManager->deleteResource(req);
        break;
    case SxQuery::HEAD:
        reply = mNetworkAccessManager->head(req);
    }

    if (seccondAttempt)
        reply->setProperty("seccondAttempt", true);
    reply->setProperty("startTime", QDateTime::currentDateTime());

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(reply, &QNetworkReply::sslErrors, [this, timer, reply](const QList<QSslError> &errors) {
       if (checkSsl(reply, timer, errors))
           reply->ignoreSslErrors();
    });
    connect(timer, &QTimer::timeout, [this, reply]() {
        if (!reply->isFinished()) {
            reply->abort();
            this->storeReplyTime(reply);
        }
    });
    connect(reply, &QNetworkReply::bytesWritten, [this, reply, timer]() {
        timer->start(mTimeoutProgress);
        this->storeReplyTime(reply);
    });
    connect(reply, &QNetworkReply::downloadProgress, [this, reply, timer]() {
        timer->start(mTimeoutProgress);
        this->storeReplyTime(reply);
    });
    mActiveTimers.insert(reply, timer);
    timer->start(mTimeoutInitial);
    return reply;
}

void SxCluster::tryRemoveNetworkAccessManager(QNetworkAccessManager *manager)
{
    logEntry("");
    bool canDelete = true;
    foreach (QNetworkReply* reply, mActiveQueries.keys()) {
        if (reply->manager() == manager) {
            canDelete = false;
            break;
        }
    }
    if (canDelete) {
        mNetworkManagersToRemove.remove(manager);
        manager->deleteLater();
    }
    else
        mNetworkManagersToRemove.insert(manager);
}

bool SxCluster::_filter_data_process(QFile *inFile, QFile *outFile, SxFilter *filter, QString file, bool download)
{
    logEntry("");
    Q_UNUSED(file);
	const int buff_size = 8192;
	std::unique_ptr<char[]> inbuff(new char[buff_size]);
	std::unique_ptr<char[]> outbuff(new char[buff_size]);

    //char *inbuff, *outbuff;
    qint64 bread, bwrite;
    sxf_action_t action = SXF_ACTION_NORMAL;

    while ((bread = inFile->read(inbuff.get(), buff_size))>0) {
        if (QCoreApplication::instance()->thread() == QThread::currentThread()){
            QEventLoop loop;
            loop.processEvents(QEventLoop::AllEvents, 10);
        }
        if (aborted())
            return false;
        if (inFile->pos()==inFile->size())
            action = SXF_ACTION_DATA_END;
        do {
            bwrite = filter->dataProcess(inbuff.get(), bread, outbuff.get(), buff_size, download ? SXF_MODE_DOWNLOAD : SXF_MODE_UPLOAD, &action);
            if (bwrite < 0) {
                QString errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Filter %1 dataProcess failed");
                mLastError = SxError(SxErrorCode::FilterError,
                                     errorMessage.arg(filter->shortname()),
                                     QCoreApplication::translate("SxErrorMessage",  errorMessage.toUtf8().constData()).arg(filter->shortname()));
                return false;
            }
            if (outFile->write(outbuff.get(), bwrite) != bwrite) {
                QString errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Failed to write file %1");
                mLastError = SxError(SxErrorCode::IOError,
                                     errorMessage.arg(outFile->fileName()),
                                     QCoreApplication::translate("SxErrorMessage",  errorMessage.toUtf8().constData()).arg(outFile->fileName()));
                return false;
            }
        }
        while (action == SXF_ACTION_REPEAT);
    }
    if (!outFile->flush()) {
        QString errorMessage = QT_TRANSLATE_NOOP("SxErrorMessage", "Failed to flush file %1");
        mLastError = SxError(SxErrorCode::IOError,
                             errorMessage.arg(outFile->fileName()),
                             QCoreApplication::translate("SxErrorMessage",  errorMessage.toUtf8().constData()).arg(outFile->fileName()));
    }
    return true;
}

void SxCluster::storeReplyTime(QNetworkReply *reply)
{
    auto now = QDateTime::currentDateTime();
    QVariant property = reply->property("progressTime");
    if (property.isValid()) {
        int t = static_cast<int>(property.toDateTime().msecsTo(now));
        if (t>mProgressReplyTimeMax)
            mProgressReplyTimeMax = t;
        ++mProgressReplyTimeCount;
        if (mProgressReplyTimeCount>=10) {
            mProgressReplyTimes.append(mProgressReplyTimeMax);
            mProgressReplyTimeMax = mProgressReplyTimeCount = 0;
            while (mProgressReplyTimes.count()>10)
                mProgressReplyTimes.removeFirst();
            int max = 0;
            foreach (int time, mProgressReplyTimes) {
                if (time > max)
                    max = time;
            }
            mTimeoutProgress = max*mTimeoutMultiplier;
            if (mTimeoutProgress < sTimeoutProgress*1000)
                mTimeoutProgress = sTimeoutProgress*1000;
        }
        else {
            if (t*mTimeoutMultiplier>mTimeoutProgress)
                mTimeoutProgress = t*mTimeoutMultiplier;
        }
    }
    else {
        property = reply->property("startTime");
        if (!property.isValid())
            return;
        int t = static_cast<int>(property.toDateTime().msecsTo(now));
        if (t>mInitialReplyTimeMax)
            mInitialReplyTimeMax = t;
        ++mInitialReplyTimeCount;
        if (mInitialReplyTimeCount>=10) {
            mInitialReplyTimes.append(mInitialReplyTimeMax);
            mInitialReplyTimeMax = mInitialReplyTimeCount = 0;
            while (mInitialReplyTimes.count()>10)
                mInitialReplyTimes.removeFirst();
            int max = 0;
            foreach (int time, mInitialReplyTimes) {
                if (time > max)
                    max = time;
            }
            mTimeoutInitial = max*mTimeoutMultiplier;
            if (mTimeoutInitial < sTimeoutInitial*1000)
                mTimeoutInitial = sTimeoutInitial*1000;
        }
        else {
            if (t*mTimeoutMultiplier>mTimeoutInitial)
                mTimeoutInitial = t*mTimeoutMultiplier;
        }
    }
    reply->setProperty("progressTime", now);
}

bool SxCluster::aborted() const
{
    QMutexLocker locker(&mAbortedMutex);
    return m_Aborted;
}

void SxCluster::setAborted(bool aborted)
{
    QMutexLocker locker(&mAbortedMutex);
    m_Aborted = aborted;
}

bool SxCluster::checkNetworkConfigurationChanged()
{
    if (mNetworkConfiguration == QNetworkInterface::allAddresses()) {
        return false;
    }
    mNetworkConfiguration = QNetworkInterface::allAddresses();
    if (mUseApplianceNodeList) {
        bool needReinit = false;
        foreach (QString ip, mNodeList) {
            QString uuid, error;
            if (!getClusterUUID(mSxAuth.clusterName(), ip, mSxAuth.use_ssl(), mSxAuth.port(), uuid, error)) {
                needReinit = true;
                break;
            }
            if (uuid != mClusterUuid) {
                needReinit = true;
                break;
            }
        }
        if (needReinit) {
            mUseApplianceNodeList = false;
            QStringList nodes;
            if (mSxAuth.initialAddress().isEmpty()) {
                QHostInfo hostInfo = QHostInfo::fromName(mSxAuth.clusterName());
                foreach (QHostAddress address, hostInfo.addresses()) {
                    nodes << address.toString();
                }
            }
            else {
                nodes << mSxAuth.initialAddress();
            }
            mNodeList = nodes;
            return true;
        }
        return false;
    }
    else {
        if (!reloadClusterMeta())
            return false;
        if (mMeta.contains("appliance_ip_list")) {
            auto value = QString::fromUtf8(mMeta.value("appliance_ip_list"));
            logInfo("appliance_ip_list: "+value);

            QStringList ip_list = value.split(";", QString::SkipEmptyParts);
            if (!ip_list.isEmpty()) {
                bool usePrivateIp = true;
                foreach (QString ip, ip_list) {
                    logInfo("testing "+ip);
                    QString uuid, errorMessage;
                    if (!getClusterUUID(mSxAuth.clusterName(), ip, mSxAuth.use_ssl(), mSxAuth.port(), uuid, errorMessage)) {
                        logInfo("node "+ip+" unreachable");
                        usePrivateIp = false;
                        break;
                    }
                    if (uuid != mClusterUuid) {
                        logInfo("node "+ip+": cluster UUID mismatch");
                        usePrivateIp = false;
                        break;
                    }
                }
                if (usePrivateIp) {
                    mNodeList = ip_list;
                    mUseApplianceNodeList = true;
                    return true;
                }
            }
        }
        return false;
    }
}

void SxCluster::abortAllQueries()
{
    logEntry("");
    setAborted(true);
    foreach (QNetworkReply *reply, mActiveTimers.keys()) {
        if (reply->isFinished()) {
            continue;
        }
        QTimer *timer = mActiveTimers.value(reply);
        timer->stop();
        reply->disconnect();
        reply->abort();
        connect(reply, &QNetworkReply::finished, [reply, timer, this]() {
            if (reply->manager() != mNetworkAccessManager)
                tryRemoveNetworkAccessManager(reply->manager());
            reply->deleteLater();
            timer->deleteLater();
            mActiveTimers.remove(reply);
            mActiveQueries.remove(reply);
        });
    }
    emit sig_exit_loop();
}

QPair<SxQuery *, SxQueryResult *> SxCluster::querySelect(QHash<SxQuery *, QStringList*> &queries, const QString &etag)
{
    logEntry("");
    setAborted(false);
    QEventLoop loop;
    connect(this, &SxCluster::sig_exit_loop, &loop, &QEventLoop::quit);
    QNetworkReply* currentReply = nullptr;
    SxQuery *currentQuerry = nullptr;
    QSet<QString> activeTargets;
    std::unique_ptr<SxQueryResult> result(new SxQueryResult());

    startFunction:
    if (queries.isEmpty()) {
        return QPair<SxQuery *, SxQueryResult *>(nullptr, nullptr);
    }

    foreach (QNetworkReply* r, mActiveQueries.keys()) {
        if (r->isFinished()) {
            QTimer *timer = mActiveTimers.take(r);
            if (timer) {
                timer->stop();
                timer->disconnect();
                delete timer;
            }
            currentReply = r;
            currentQuerry = mActiveQueries.take(currentReply);
            if (r->manager() != mNetworkAccessManager)
                tryRemoveNetworkAccessManager(r->manager());
            goto processNetworkReply;
        }
    }

    sendQueries:
    activeTargets.clear();
    foreach (SxQuery *query, queries.keys()) {
        QNetworkReply* reply;
        if (mActiveQueries.values().contains(query)) {
            reply = mActiveQueries.key(query);
        }
        else {
            QStringList* targetList = queries.value(query);
            if (targetList->isEmpty()) {
                abortAllQueries();
                return QPair<SxQuery*, SxQueryResult*>(query, result.release());
            }
            QString target = targetList->takeFirst();
            int delay = 0;
            if (activeTargets.contains(target)) {
                activeTargets.clear();
                delay = 50;
            }
            QNetworkRequest req = query->makeRequest(target, mSxAuth, mTimeDrift, etag);
            reply = sendNetworkRequest(query, req, false, delay);
            mActiveQueries.insert(reply, query);
            activeTargets.insert(target);
        }
        connect(reply, &QNetworkReply::finished, [&loop, this, reply, &currentReply]() {
           QTimer* timer = mActiveTimers.take(reply);
           timer->stop();
           delete timer;
           currentReply = reply;
           loop.exit(0);
        });
    }
    loop.exec();
    if (aborted()) {
        foreach (QNetworkReply* reply, mActiveQueries.keys()) {
            auto timer = mActiveTimers.take(reply);
            mActiveQueries.remove(reply);
            reply->deleteLater();
            if (timer != nullptr)
                timer->deleteLater();
        }
        return QPair<SxQuery *, SxQueryResult *>(nullptr, new SxQueryResult("", 0, SxError(SxErrorCode::AbortedByUser, "", ""), QByteArray{}, false, ""));
    }

    if (currentReply == nullptr) {
        return QPair<SxQuery *, SxQueryResult *>(nullptr, nullptr);
    }

    currentQuerry = mActiveQueries.take(currentReply);
    foreach (QNetworkReply *reply, mActiveQueries.keys()) {
        disconnect(reply, &QNetworkReply::finished, 0, 0);
    }
    if (currentReply->manager() != mNetworkAccessManager)
        tryRemoveNetworkAccessManager(currentReply->manager());

    processNetworkReply:
    currentReply->deleteLater();
    if(currentReply->hasRawHeader("Date")) {
        QDateTime dt = QDateTime::fromString(currentReply->rawHeader("Date"), Qt::RFC2822Date);
        if(dt.isValid())
            mTimeDrift = QDateTime::currentDateTime().secsTo(dt);
    }
    result.reset(processReply(currentReply, mClusterUuid));
    if (result->error().errorCode() == SxErrorCode::SslError && result->error().errorMessage()=="NULL certificate") {
        if (!currentReply->property("seccondAttempt").isValid()) {
            logWarning(QString("Peer cert is null, retrying to the same node: %1 (%2)").arg(result->host()).arg(currentQuerry->number));
            currentReply->disconnect();
            QNetworkRequest req = currentReply->request();
            if (currentReply->manager() == mNetworkAccessManager) {
                tryRemoveNetworkAccessManager(mNetworkAccessManager);
                mNetworkAccessManager = new QNetworkAccessManager(this);
            }
            QNetworkReply *reply = sendNetworkRequest(currentQuerry, req, true);
            mActiveQueries.insert(reply, currentQuerry);
            currentReply = nullptr;
            currentQuerry = nullptr;
            goto sendQueries;
        }
        else {
            logWarning(QString("Query %1 failed again").arg(currentQuerry->number));
        }
    }
    if (result->error().errorCode() == SxErrorCode::NoError || result->error().errorCode() == SxErrorCode::NotChanged) {
        return QPair<SxQuery *, SxQueryResult *>(currentQuerry, result.release());
    }
    static const QList<SxErrorCode> finishOnError = {
        SxErrorCode::InvalidCredentials,
        SxErrorCode::CriticalServerError
    };
    if (finishOnError.contains(result->error().errorCode())) {
        abortAllQueries();
        return QPair<SxQuery *, SxQueryResult *>(currentQuerry, result.release());
    }
    if (result->error().errorCode() == SxErrorCode::Timeout) {
        logWarning(QString("Connection to %1 timeout (%2)").arg(result->host()).arg(currentQuerry->number));
        if (currentReply->manager() == mNetworkAccessManager) {
            tryRemoveNetworkAccessManager(mNetworkAccessManager);
            mNetworkAccessManager = new QNetworkAccessManager(this);
        }
        foreach (QTimer* timer, mActiveTimers.values()) {
            if (timer->isActive()) {
                int remaining = timer->remainingTime() + 5000;
                timer->start(remaining);
            }
        }
        goto sendQueries;
    }
    if (result->error().errorCode() == SxErrorCode::NetworkError) {
        if (currentReply->manager() == mNetworkAccessManager) {
            logWarning("Try to restart NetworkAccessManager");
            tryRemoveNetworkAccessManager(mNetworkAccessManager);
            mNetworkAccessManager = new QNetworkAccessManager(this);
        }
    }
    QStringList* targetList = queries.value(currentQuerry);
    if (!targetList->isEmpty())
        goto startFunction;

    if (!result)
        result.reset(new SxQueryResult());
    mLastError = result->error();
    return QPair<SxQuery *, SxQueryResult *>(currentQuerry, result.release());
}

bool SxCluster::testVolume(SxVolume *volume)
{
    if (!volume || volume->name().isEmpty()) {
        mLastError = SxError(SxErrorCode::InvalidArgument, "invalid volume", QCoreApplication::translate("SxErrorMessage",  "invalid volume"));
        logWarning(mLastError.errorMessage());
        return false;
    }
    return true;
}

bool SxCluster::testFile(SxFile &file)
{
    if (!testVolume(file.mVolume))
        return false;

    if (file.mLocalPath.isEmpty() || file.mLocalPath == "/") {
        mLastError = SxError(SxErrorCode::InvalidArgument, "invalid path", QCoreApplication::translate("SxErrorMessage",  "invalid path"));
        logWarning(mLastError.errorMessage());
        return false;
    }
    //TODO add support for filename processing filters
    return true;
}

bool SxCluster::parseJson(SxQueryResult *queryResult, QJsonDocument &json, bool silence)
{
    bool error = false;
    if (queryResult->error().errorCode() == SxErrorCode::InvalidCredentials) {
        mLastError = queryResult->error();
        return false;
    }
    if (queryResult->error().errorCode() != SxErrorCode::NoError) {
        error = true;
    }
    QJsonParseError jsonErr;
    json = QJsonDocument::fromJson(queryResult->data(), &jsonErr);
    if(jsonErr.error != QJsonParseError::NoError) {
        mLastError = queryResult->error();
        if (error) {
            if (mLastError.errorMessage() != "Nothing changes" && !silence)
                logWarning(mLastError.errorMessage());
            return false;
        }
        if (!silence)
            logWarning(mLastError.errorMessage());
        return false;
    }

    if (json.object().value("ErrorId").isString()) {
        error = true;
        if (!silence)
            logWarning("ErrorId: " + json.object().value("ErrorId").toString());
    }
    if(json.object().value("ErrorMessage").isString()) {
        error = true;
        QString errorMessage = json.object().value("ErrorMessage").toString();
        //TODO detect proper error code

        SxErrorCode errorCode = SxErrorCode::UnknownError;
        auto httpCode = queryResult->returnCode();
        if (httpCode == 404)
            errorCode = SxErrorCode::NotFound;
        else if (httpCode == 413)
            errorCode = SxErrorCode::OutOfSpace;

        mLastError = SxError(errorCode, errorMessage, errorMessage);
        if (!silence)
            logWarning("ErrorMessage:" + json.object().value("ErrorMessage").toString());
    }
    if(json.object().value("ErrorDetails").isString()) {
        error = true;
        if (!silence)
            logWarning("ErrorDetails:" + json.object().value("ErrorDetails").toString());
    }
    if (error)
        return false;
    return true;
}

bool SxCluster::parseJobJson(QJsonDocument &json, QString pollTarget, SxJob &job)
{
    if (json.object().value("requestId").toString("").isEmpty() ||
            json.object().value("minPollInterval").toInt(0) <= 0 ||
            json.object().value("maxPollInterval").toInt(0) <= 0)
    {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    job.mStatus = SxJob::PENDING;
    job.mMinPollInterval = json.object().value("minPollInterval").toInt();
    job.mInterval = job.mMinPollInterval;
    job.mMaxPollInterval = json.object().value("maxPollInterval").toInt();
    job.mRequestId = json.object().value("requestId").toString();
    job.mTarget = pollTarget;
    return true;
}

bool SxCluster::checkSsl(QNetworkReply *reply, QTimer* , const QList<QSslError> &errors)
{
    QSslCertificate peerCert = reply->sslConfiguration().peerCertificate();
    if(peerCert.isNull())
    {
        foreach (QSslCertificate cert, reply->sslConfiguration().peerCertificateChain()) {
            if (!cert.isNull()) {
                peerCert = cert;
                break;
            }
        }
        if (peerCert.isNull()) {
            logWarning("checkSSL: peer cert is null, host:" + reply->request().url().host());
            reply->setProperty("lastSslError", "NULL certificate");
            return false;
        }
    }
    for(int i=0; i<errors.size(); i++) {
        if(errors.at(i).error() == QSslError::HostNameMismatch) {
            QString errorMessage;
            if(!isCertFor(peerCert, mSxAuth.clusterName(), &errorMessage)) {
                if (errorMessage.size())
                    reply->setProperty("lastSslError", errorMessage);
                logWarning("SSL error: cluster name mismatch");
                return false;
            }
        } else if(errors.at(i).error() == QSslError::SelfSignedCertificate ||
                  errors.at(i).error() == QSslError::SelfSignedCertificateInChain ||
                  errors.at(i).error() == QSslError::CertificateUntrusted) {
            QCryptographicHash sha1(QCryptographicHash::Sha1);
            sha1.addData(peerCert.toDer());
            QByteArray fprint = sha1.result();

            QByteArray storedFingerprint = mUseApplianceNodeList ? m_applianceCertFprint : m_certFprint;

            if (storedFingerprint.isEmpty() && mCallbackConfirmCert)
            {
                QList<QTimer*> stoppedTimers;
                foreach (QTimer *t, mActiveTimers.values()) {
                    if (t->isActive()) {
                        t->stop();
                        stoppedTimers.append(t);
                    }
                }
                if (mCallbackConfirmCert(peerCert, mUseApplianceNodeList)) {
                    storedFingerprint = fprint;
                    if (mUseApplianceNodeList)
                        m_applianceCertFprint = fprint;
                    else
                        m_certFprint = fprint;
                }
                foreach (QTimer *t, stoppedTimers) {
                    t->start();
                }
            }
            if(fprint == storedFingerprint)
                continue;
            logWarning("The peer certificate is not trusted (SHA1:" + fprint.toHex() + ")");
            return false;
        } else {
            logWarning("SSL error: " + errors.at(i).errorString());
            return false;
        }
    }
    return true;
}

bool SxCluster::_listNodes(QStringList& nodeList)
{
    logEntry("");
    QStringList newNodes;
    SxQuery query("/?nodeList", SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    {
        if(!json.object().value("nodeList").isArray()) {
            goto badReplyContent;
        }
        QJsonArray nodes = json.object().value("nodeList").toArray();
        foreach (QJsonValue jNode, nodes) {
            if (jNode.isString())
                newNodes << jNode.toString();
            else {
                logWarning("Got bad reply from cluster when listing nodes");
                newNodes.clear();
                return false;
            }
        }
    }
    nodeList = newNodes;
    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_getUserDetails(SxUserInfo &userInfo)
{
    logEntry("");
    SxQuery query("/.self", SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    {
        if (json.object().keys().count() != 1) {
            goto badReplyContent;
        }
        QString username = json.object().keys().first();
        QJsonObject jUser = json.object().value(username).toObject();
        if (!jUser.value("userQuota").isDouble() ||
            !jUser.value("userQuotaUsed").isDouble() ||
            !jUser.value("userDesc").isString())
        {
            goto badReplyContent;
        }
        qint64 quota = static_cast<qint64>(jUser.value("userQuota").toDouble());
        qint64 quotaUsed = static_cast<qint64>(jUser.value("userQuota").toDouble());
        QString desc = jUser.value("userDesc").toString();
        userInfo.mUsername = username;
        userInfo.mQuota = quota;
        userInfo.mQuotaUsed = quotaUsed;
        userInfo.mDesc.clear();

        QJsonParseError jsonErr;
        json = QJsonDocument::fromJson(desc.toUtf8(), &jsonErr);
        if (jsonErr.error == QJsonParseError::NoError && json.isObject()) {
            foreach (QString key, json.object().keys()) {
                QJsonValue value = json.object().value(key);
                userInfo.mDesc.insert(key, value);
            }
        }
    }
    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_listVolumes(QList<SxVolume *> &volumeList)
{
    logEntry("");
    SxQuery query("/?volumeList", SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    {
        if (!json.object().value("volumeList").isObject()) {
            goto badReplyContent;
        }

        QJsonObject jList = json.object().value("volumeList").toObject();

        foreach (QString volume, jList.keys()) {
            QJsonObject jVolume = jList.value(volume).toObject();
            if (!jVolume.value("owner").isString() ||
                    !jVolume.value("privs").isString() ||
                    !jVolume.value("usedSize").isDouble() ||
                    !jVolume.value("sizeBytes").isDouble())
            {
                goto badReplyContent;
            }
        }

        foreach (SxVolume* volume, volumeList) {
            if (!jList.keys().contains(volume->name())) {
                delete volume;
                volumeList.removeOne(volume);
            }
        }

        auto iterator = jList.begin();
        while (iterator != jList.end()) {
            QString volume = iterator.key();
            QJsonObject jVolume = iterator.value().toObject();
            SxVolume *sxvol = 0;
            foreach (SxVolume* v, volumeList) {
                if (v->name() == volume) {
                    sxvol = v;
                    break;
                }
            }
            QString owner = jVolume.value("owner").toString();
            qint64 size = static_cast<qint64>(jVolume.value("sizeBytes").toDouble());
            qint64 usedSize = static_cast<qint64>(jVolume.value("usedSize").toDouble());
            QString privs = jVolume.value("privs").toString();
            bool canRead = privs.at(0)=='r';
            bool canWrite = privs.at(1)=='w';
            QString globalId = jVolume.value("globalID").toString("");

            if (!sxvol) {
                sxvol = new SxVolume(this, volume, owner, size, usedSize, canRead, canWrite, globalId);
                volumeList.append(sxvol);
            }
            else {
                sxvol->setUsedSize(usedSize);
                sxvol->setPerms(canRead, canWrite);
                sxvol->setGlobalId(globalId);
            }
            iterator++;
        }
    }
    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_locateVolume(SxVolume *volume, qint64 fileSize, int *blockSize)
{
    logEntry("");
    if (!testVolume(volume))
        return false;
    QString queryString = "/"+volume->name()+"?o=locate&volumeMeta&customVolumeMeta";
    if (fileSize && blockSize) {
        queryString+="&size="+QString::number(fileSize);
    }
    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    {
        if (!json.object().value("nodeList").isArray())
            goto badReplyContent;
        QStringList nodeList;
        int tmpBlockSize = 0;
        foreach (QJsonValue node, json.object().value("nodeList").toArray()) {
            if (!node.isString())
                goto badReplyContent;
            nodeList << node.toString();
        }
        if (fileSize && blockSize) {
            if (!json.object().value("blockSize").isDouble())
                goto badReplyContent;
            tmpBlockSize = json.object().value("blockSize").toInt();
        }
        if (json.object().value("volumeMeta").isObject()) {
            QJsonObject jMeta = json.object().value("volumeMeta").toObject();
            foreach (QString key, jMeta.keys()) {
                if (!jMeta.value(key).isString())
                    goto badReplyContent;
            }
        }
        if (json.object().value("customVolumeMeta").isObject()) {
            QJsonObject jMeta = json.object().value("customVolumeMeta").toObject();
            foreach (QString key, jMeta.keys()) {
                if (!jMeta.value(key).isString())
                    goto badReplyContent;
            }
        }
        volume->setNodeList(nodeList);
        if (fileSize && blockSize)
            *blockSize = tmpBlockSize;

        uint configCounter1 = volume->meta().changeCounter() + volume->customMeta().changeCounter();

        if (json.object().value("sizeBytes").isDouble() && json.object().value("usedSize").isDouble()) {
            qint64 size     = json.object().value("sizeBytes").toVariant().toLongLong();
            qint64 usedSize = json.object().value("usedSize").toVariant().toLongLong();
            volume->setSize(size);
            volume->setUsedSize(usedSize);
        }

        if (json.object().value("volumeMeta").isObject()) {
            QJsonObject jMeta = json.object().value("volumeMeta").toObject();
            foreach (QString key, volume->meta().keys()) {
                if (!jMeta.contains(key))
                    volume->meta().removeKey(key);
            }
            foreach (QString key, jMeta.keys()) {
                volume->meta().setValue(key, QByteArray::fromHex(jMeta.value(key).toString().toUtf8()));
            }
        }
        if (json.object().value("customVolumeMeta").isObject()) {
            QJsonObject jMeta = json.object().value("customVolumeMeta").toObject();
            foreach (QString key, volume->customMeta().keys()) {
                if (!jMeta.contains(key))
                    volume->customMeta().removeKey(key);
            }
            foreach (QString key, jMeta.keys()) {
                volume->customMeta().setValue(key, QByteArray::fromHex(jMeta.value(key).toString().toUtf8()));
            }
        }
        uint configCounter2 = volume->meta().changeCounter() + volume->customMeta().changeCounter();
        if (configCounter1 != configCounter2) {
            QHash<QString, QByteArray> meta;
            QHash<QString, QByteArray> customMeta;
            foreach (QString key, volume->meta().keys()) {
                meta.insert(key, volume->meta().value(key));
            }
            foreach (QString key, volume->customMeta().keys()) {
                customMeta.insert(key, volume->customMeta().value(key));
            }
            VolumeConfigWatcher::instance()->emitConfigChanged(volume->name(), meta, customMeta);
        }
    }
    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_getClusterMetadata(SxMeta &clusterMeta)
{
    logEntry("");
    SxQuery query("/?clusterMeta", SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    {
        if (!json.object().value("clusterMeta").isObject())
            goto badReplyContent;
        QJsonObject jMeta = json.object().value("clusterMeta").toObject();
        foreach (QString key, jMeta.keys()) {
            if (!jMeta.value(key).isString())
                goto badReplyContent;
        }
        foreach (QString key, clusterMeta.keys()) {
            if (!jMeta.contains(key))
                clusterMeta.removeKey(key);
        }
        foreach (QString key, jMeta.keys()) {
            clusterMeta.setValue(key, QByteArray::fromHex(jMeta.value(key).toString().toUtf8()));
        }
    }
    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_listFiles(SxVolume *volume, QList<SxFileEntry *> &fileList, QString &etag, const QString &after, const qint64 limit)
{
    return _listFiles(volume, "", true, fileList, etag, after, limit);
}

bool SxCluster::_listFiles(SxVolume *volume, const QString pathFilter, bool recursive, QList<SxFileEntry *> &fileList, QString &etag, const QString &after, const qint64 limit)
{
    logEntry("");
    if (!testVolume(volume))
        return false;
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;

    std::unique_ptr<SxFilter> filter(SxFilter::getActiveFilter(volume));
    foreach (SxFileEntry* entry, fileList) {
        delete entry;
    }
    fileList.clear();
    QString queryString = "/"+volume->name()+"?o=list";
    if (filter && filter->filemetaProcess()) {
        queryString+="&recursive&meta";
    }
    else {
        if (!pathFilter.isEmpty() && pathFilter != "/") {
            queryString += "&filter="+QUrl::toPercentEncoding(pathFilter, "/");
        }
        if (recursive)
            queryString+="&recursive";
        if (!after.isEmpty() && limit>0){
            QString afterTmp;
            if (after.startsWith("/"))
                afterTmp = after.mid(1);
            else
                afterTmp = after;
            queryString+=QString("&limit=%1&after=%2").arg(limit).arg(afterTmp);
        }
    }
    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, volume->nodeList(), etag));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;

    {
        QList<SxFileEntry *> directories;
        QSet<QString> directoriesInEncryptedDir;

        if (!json.object().value("fileList").isObject())
            goto badReplyContent;
        QJsonObject jFileList = json.object().value("fileList").toObject();
        auto iterator = jFileList.begin();
        while (iterator != jFileList.end()) {
            QString path = iterator.key();
            if (recursive || !path.endsWith("/")) {
                QJsonObject jFileEntry = iterator.value().toObject();
                if (!jFileEntry.value("fileSize").isDouble() ||
                        !jFileEntry.value("blockSize").isDouble() ||
                        !jFileEntry.value("createdAt").isDouble() ||
                        !jFileEntry.value("fileRevision").isString() ) {
                    goto badReplyContent;
                }
                SxFileEntry *entry = new SxFileEntry();
                if (!recursive && path.endsWith("/.sxnewdir"))
                {
                   delete entry;
                   goto endBlock;
                }
                entry->mPath = path;
                entry->mSize = jFileEntry.value("fileSize").toVariant().toLongLong();
                entry->mCreatedAt = jFileEntry.value("createdAt").toVariant().toUInt();
                entry->mBlockSize = jFileEntry.value("blockSize").toInt();
                entry->mRevision = jFileEntry.value("fileRevision").toString();

                if (filter && filter->filemetaProcess()) {
                    SxFile f(volume, entry->mPath, entry->mRevision, false);
                    if (jFileEntry.value("fileMeta").isObject()) {
                        if (!_setFileMetadata(f, jFileEntry.value("fileMeta").toObject()))
                            goto clean;
                    }
                    else {
                        if (!_getFileMetadata(f))
                            goto clean;
                    }
                    if (!filter->filemetaProcess(f, false)) {
                        QString message = "Filter %1 filemetaProcess failed: %2";
                        mLastError = SxError(SxErrorCode::FilterError, message.arg(filter->shortname()).arg(filter->lastWarning()),
                                             QCoreApplication::translate("SxErrorMessage",  message.toUtf8().constData()).arg(filter->shortname()).arg(filter->lastWarning()));
                        goto clean;
                    }
                    auto t4 = QDateTime::currentDateTime();
                    entry->mPath = f.mLocalPath;
                    if (recursive) {
                        if (pathFilter.isEmpty() || pathFilter == entry->path())
                            fileList.append(entry);
                        else {
                            QString tmpFilter = pathFilter;
                            if (!pathFilter.endsWith('/'))
                                tmpFilter += '/';
                            if (entry->path().startsWith(tmpFilter))
                                fileList.append(entry);
                            else
                                delete entry;
                        }
                    }
                    else {
                        if (pathFilter.isEmpty())
                            delete entry;
                        else if (pathFilter == entry->path())
                            fileList.append(entry);
                        else {
                            QString tmpFilter = pathFilter;
                            if (!pathFilter.endsWith('/'))
                                tmpFilter += '/';
                            if (entry->path().startsWith(tmpFilter)) {

                                int index = entry->path().indexOf('/', tmpFilter.length());
                                if (index >= 0) {
                                    QString dir = entry->path().mid(0, index+1);
                                    if (directoriesInEncryptedDir.contains(dir))
                                        delete entry;
                                    else {
                                        entry->mPath = dir;
                                        entry->mSize = 0;
                                        entry->mCreatedAt = 0;
                                        entry->mBlockSize = 0;
                                        entry->mRevision = "";
                                        directories.append(entry);
                                        directoriesInEncryptedDir.insert(dir);
                                    }
                                }
                                else {
                                    if (entry->path().endsWith("/.sxnewdir"))
                                        delete entry;
                                    else
                                        fileList.append(entry);
                                }
                            }
                            else
                                delete entry;
                        }
                    }
                }
                else
                    fileList.append(entry);
            }
            else {
                SxFileEntry *entry = new SxFileEntry();
                entry->mPath = path;
                entry->mSize = 0;
                entry->mCreatedAt = 0;
                entry->mBlockSize = 0;
                entry->mRevision = "";
                /*
                if (filter && filter->filemetaProcess()) {
                    // THIS CODE WILL NEVER BE CALLED !!!
                    SxFile f(volume, entry->mPath, entry->mRevision, false);
                    if (!_getFileMetadata(f)) {
                        goto clean;
                    }
                    if (!filter->filemetaProcess(f, false)) {
                        QString message = "Filter %1 filemetaProcess failed: %2";
                        mLastError = SxError(SxErrorCode::FilterError, message.arg(filter->shortname()).arg(filter->lastWarning()),
                                             QCoreApplication::translate("SxErrorMessage",  message.toUtf8().constData()).arg(filter->shortname()).arg(filter->lastWarning()));
                        goto clean;
                    }
                    entry->mPath = f.mLocalPath;
                }
                */
                directories.append(entry);
            }
            endBlock:
            iterator++;
        }
        if (filter && filter->filemetaProcess()) {
            auto lessThan = [](SxFileEntry *e1, SxFileEntry *e2) -> bool {
                return e1->path() < e2->path();
            };
            qSort(directories.begin(), directories.end(), lessThan);
            qSort(fileList.begin(), fileList.end(), lessThan);
        }
        if (!directories.isEmpty()) {
            fileList = directories + fileList;
        }
        etag = queryResult->etag();
    }

    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    clean:
    foreach (SxFileEntry* entry, fileList) {
        delete entry;
    }
    fileList.clear();
    return false;
}

bool SxCluster::_getFile(SxFile &file, bool silence)
{
    logEntry("");
    if(!testFile(file))
        return false;

    QString queryString = "/"+file.mVolume->name();
    if (file.mRemotePath.startsWith("/"))
        queryString += QUrl::toPercentEncoding(file.mRemotePath, "/");
    else
        queryString += "/"+QUrl::toPercentEncoding(file.mRemotePath, "/");
    if (!file.mRevision.isEmpty())
        queryString += "?rev="+file.mRevision;

    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, file.mVolume->nodeList()));
    if (!queryResult)
        return false;

    QJsonDocument json;
    if (!parseJson(queryResult.get(), json, silence))
        return false;

    {
        if (!json.object().value("blockSize").isDouble() ||
                !json.object().value("createdAt").isDouble() ||
                !json.object().value("fileSize").isDouble() ||
                !json.object().value("fileRevision").isString() ||
                !json.object().value("fileData").isArray())
        {
            goto badReplyContent;
        }
        int blockSize = json.object().value("blockSize").toInt();
        int createdAt = json.object().value("createdAt").toInt();
        QString rev = json.object().value("fileRevision").toString();
        qint64 size = static_cast<qint64>(json.object().value("fileSize").toDouble());

        qint64 blockCount = size/blockSize + (size%blockSize?1:0);
        QJsonArray jFileData = json.object().value("fileData").toArray();
        if (jFileData.count()!=blockCount)
            goto badReplyContent;
        file.clearBlocks();
        foreach (auto jBlock, jFileData) {
            if (jBlock.toObject().keys().count() != 1)
                goto badReplyContent;
            QString blockHash = jBlock.toObject().keys().first();
            QStringList blockNodes;
            if (!jBlock.toObject().value(blockHash).isArray()
                    || jBlock.toObject().value(blockHash).toArray().count()==0)
                goto badReplyContent;
            foreach (auto jNode, jBlock.toObject().value(blockHash).toArray()) {
                if (!jNode.isString() || jNode.toString().isEmpty())
                    goto badReplyContent;
                blockNodes.append(jNode.toString());
            }
            file.appendBlock(blockHash, blockNodes);
        }
        file.mRevision = rev;
        file.mRemoteSize = size;
        file.mCreatedAt = createdAt;
        file.mBlockSize = blockSize;
    }

    return true;
    badReplyContent:
    file.clearBlocks();
    mLastError = SxError::errorBadReplyContent();
    if (!silence)
        logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_getFileMetadata(SxFile &file)
{
    logEntry("");
    if(!testFile(file))
        return false;

    QString queryString = "/"+file.mVolume->name();
    if (file.mRemotePath.startsWith("/"))
        queryString += QUrl::toPercentEncoding(file.mRemotePath, "/");
    else
        queryString += "/"+QUrl::toPercentEncoding(file.mRemotePath, "/");
    queryString += "?fileMeta";

    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, file.mVolume->nodeList()));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;

    QJsonObject jMeta;
    if (!json.object().value("fileMeta").isObject())
        goto badReplyContent;
    jMeta = json.object().value("fileMeta").toObject();

    if (_setFileMetadata(file, jMeta))
        return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_setFileMetadata(SxFile &file, const QJsonObject &jMeta)
{
    foreach (QString key, jMeta.keys()) {
        if (!jMeta.value(key).isString())
            return false;
    }
    foreach (QString key, file.mMeta.keys()) {
        if (!jMeta.contains(key))
            file.mMeta.removeKey(key);
    }
    foreach (QString key, jMeta.keys()) {
        file.mMeta.setValue(key, QByteArray::fromHex(jMeta.value(key).toString().toUtf8()));
    }
    return true;
}

bool SxCluster::_initializeFile(SxFile &file)
{
    logEntry("");
    if(!testFile(file))
        return false;
    QString queryString = "/"+file.mVolume->name();
    if (file.mRemotePath.startsWith("/"))
        queryString += QUrl::toPercentEncoding(file.mRemotePath, "/");
    else
        queryString += "/"+QUrl::toPercentEncoding(file.mRemotePath, "/");

    if (file.mRemoteSize > 0 && !file.multipart())
    {
        qint64 blockCount = file.mRemoteSize/file.mBlockSize + (file.mRemoteSize%file.mBlockSize?1:0);
        if (blockCount != file.mBlocks.count()) {
            mLastError = SxError(SxErrorCode::UnknownError, "file size/blocks mismatch", QCoreApplication::translate("SxErrorMessage",  "file size/blocks mismatch"));
            logWarning(mLastError.errorMessage());
            return false;
        }
    }

    QJsonObject jObject;
    jObject.insert("fileSize", QJsonValue(file.mRemoteSize));
    if (!file.mMeta.keys().isEmpty()) {
        QJsonObject jFileMeta;
        foreach (QString key, file.mMeta.keys()) {
            QByteArray value = file.mMeta.value(key);
            jFileMeta.insert(key, QJsonValue(QString::fromUtf8(value.toHex())));
        }
        jObject.insert("", jFileMeta);
    }
    if (file.mRemoteSize > 0) {
        QJsonArray jFileData;
        foreach (SxBlock* block, file.mBlocks) {
            jFileData.append(QJsonValue(block->mHash));
        }
        jObject.insert("fileData", jFileData);
    }
    if (file.mMeta.keys().count()>0) {
        QJsonObject jMeta;
        foreach (QString key, file.mMeta.keys()) {
            QByteArray value = file.mMeta.value(key).toHex();
            jMeta.insert(key, QString::fromUtf8(value));
        }
        jObject.insert("fileMeta", jMeta);
    }

    QJsonDocument jRequest(jObject);
    SxQuery query(queryString, SxQuery::PUT, jRequest.toJson());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, file.mVolume->nodeList()));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;

    if (!json.object().value("uploadToken").isString()
            || json.object().value("uploadToken").toString().isEmpty())
        goto badReplyContent;
    if (json.object().contains("uploadData")) {
        if(!json.object().value("uploadData").isObject())
            goto badReplyContent;
        QJsonObject jUploadData = json.object().value("uploadData").toObject();
        foreach (QString hash, jUploadData.keys()) {
            SxBlock *block = file.mUniqueBlocks.value(hash, nullptr);
            if (!block)
                goto badReplyContent;
            if (!jUploadData.value(hash).isArray() ||
                    jUploadData.value(hash).toArray().count()==0)
                goto badReplyContent;
            QJsonArray jNodes = jUploadData.value(hash).toArray();
            block->mNodeList.clear();
            foreach (QJsonValue jNode, jNodes) {
                if (!jNode.isString() || jNode.toString().isEmpty())
                    goto badReplyContent;
                block->mNodeList.append(jNode.toString());
            }
            file.mBlocksToSend.append(block);
        }
    }
    file.mUploadToken = json.object().value("uploadToken").toString();
    file.mUploadPollTarget = queryResult->host();

    return true;
    badReplyContent:
    file.mBlocksToSend.clear();
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_initializeFileAddChunk(SxFile &file, int extendSeq)
{
    if(!testFile(file))
        return false;
    QString queryString = "/.upload/"+file.mUploadToken;

    QJsonObject jObject;
    jObject.insert("extendSeq", QJsonValue(extendSeq));
    QJsonArray jFileData;
    for (int i=extendSeq; i<file.mBlocks.count(); i++) {
        const SxBlock* block = file.mBlocks.at(i);
        jFileData.append(QJsonValue(block->mHash));
    }
    jObject.insert("fileData", jFileData);

    QJsonDocument jRequest(jObject);
    SxQuery query(queryString, SxQuery::PUT, jRequest.toJson());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, { file.mUploadPollTarget }));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    file.mBlocksToSend.clear();

    if (!json.object().value("uploadToken").isString()
            || json.object().value("uploadToken").toString().isEmpty())
        goto badReplyContent;
    if (json.object().contains("uploadData")) {
        if(!json.object().value("uploadData").isObject())
            goto badReplyContent;
        QJsonObject jUploadData = json.object().value("uploadData").toObject();
        foreach (QString hash, jUploadData.keys()) {
            SxBlock *block = file.mUniqueBlocks.value(hash, nullptr);
            if (!block)
                goto badReplyContent;
            if (!jUploadData.value(hash).isArray() ||
                    jUploadData.value(hash).toArray().count()==0)
                goto badReplyContent;
            QJsonArray jNodes = jUploadData.value(hash).toArray();
            block->mNodeList.clear();
            foreach (QJsonValue jNode, jNodes) {
                if (!jNode.isString() || jNode.toString().isEmpty())
                    goto badReplyContent;
                block->mNodeList.append(jNode.toString());
            }
            file.mBlocksToSend.append(block);
        }
    }
    return true;
    badReplyContent:
    file.mBlocksToSend.clear();
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::_createBlocks(const QString &uploadToken, const int blockSize, const QByteArray& data, const QStringList& nodes)
{
    logEntry("");
    QString queryString = QString("/.data/%1/%2").arg(blockSize).arg(uploadToken);
    SxQuery query(queryString, SxQuery::PUT, data);
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, nodes));
    if (!queryResult)
        return false;

    if (queryResult->error().errorCode() != SxErrorCode::NoError) {
        if (queryResult->error().errorCode() == SxErrorCode::AbortedByUser)
            mLastError = queryResult->error();
        else {
            QJsonDocument jDoc;
            parseJson(queryResult.get(), jDoc);
        }
        return false;
    }
    return true;
}

bool SxCluster::_flushFile(SxFile& file, SxJob &job)
{
    QString queryString = QString("/.upload/%1").arg(file.mUploadToken);
    SxQuery query(queryString, SxQuery::JOB_PUT, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, {file.mUploadPollTarget}));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    if (json.object().value("requestId").toString("").isEmpty() ||
            json.object().value("minPollInterval").toInt(0) <= 0 ||
            json.object().value("maxPollInterval").toInt(0) <= 0)
    {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    return parseJobJson(json, file.mUploadPollTarget, job);
}

bool SxCluster::_poll(SxJob &job)
{
    if (aborted())
        return false;
    if (job.mRequestId.isEmpty())
        return false;
    QString queryString = QString("/.results/%1").arg(job.mRequestId);
    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, {job.mTarget}));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    static const QStringList statusList = {"OK", "ERROR", "PENDING"};
    if (json.object().value("requestId").toString("")!=job.mRequestId ||
            !statusList.contains(json.object().value("requestStatus").toString("")) ||
            !json.object().value("requestMessage").isString()) {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    QString status = json.object().value("requestStatus").toString();
    QString message = json.object().value("requestMessage").toString();
    if (status == "OK")
        job.mStatus = SxJob::OK;
    else if (status == "PENDING")
        job.mStatus = SxJob::PENDING;
    else
        job.mStatus = SxJob::ERROR;
    job.mMessage = message;
    logVerbose(QString("POLL - status: %1, message=%2").arg(status,message));
    return true;
}

bool SxCluster::_deleteFile(SxFile &file, SxJob &job)
{
    logEntry("");
    if (!testFile(file))
        return false;

    QString remotePath = QString::fromUtf8(QUrl::toPercentEncoding(file.mRemotePath, "/"));
    if (remotePath.startsWith("/")) {
        remotePath = remotePath.mid(1);
    }
    QString queryString = QString("/%1/%2").arg(file.mVolume->name()).arg(remotePath);
    SxQuery query(queryString, SxQuery::JOB_DELETE, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, file.mVolume->nodeList()));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    if (json.object().value("requestId").toString("").isEmpty() ||
            json.object().value("minPollInterval").toInt(0) <= 0 ||
            json.object().value("maxPollInterval").toInt(0) <= 0)
    {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    return parseJobJson(json, queryResult->host(), job);
}

bool SxCluster::_rename(SxVolume *volume, const QString &source, const QString &destination)
{
    if (!testVolume(volume))
        return false;
    if (source.isEmpty() || source.endsWith("/"))
        return false;
    if (destination.isEmpty() || destination.endsWith("/"))
        return false;

    SxFile sourceFile(volume, source, "", true);
    SxFile destinationFile(volume, destination, "", true);
    if (!_getFile(sourceFile, false))
        return false;
    QStringList blocks;
    foreach (auto b, sourceFile.mBlocks) {
        blocks.append(b->mHash);
    }
    destinationFile.fakeFile(sourceFile.mRemoteSize, blocks, sourceFile.mBlockSize);
    if (!_initializeFile(destinationFile))
        return false;
    SxJob job;
    if (!_flushFile(destinationFile, job))
        return false;
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job))
            return false;
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    _deleteFile(sourceFile, job);
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job))
            return false;
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    return true;
}

bool SxCluster::_massRename(SxVolume *volume, const QString &source, const QString &destination, SxJob &job)
{
    if (!testVolume(volume))
        return false;
    if (source.isEmpty() || source == "/")
        return false;
    if (destination.isEmpty() || destination == "/")
        return false;
    if (source.endsWith("/") != destination.endsWith("/")) {
        logWarning("invalid arguments");
        return false;
    }

    QString queryString = QString("/%1?source=%2&dest=%3")
            .arg(volume->name())
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(source.startsWith('/') ? source.mid(1) : source, "/")))
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(destination.startsWith('/') ? destination.mid(1) : destination, "/")));
    if (source.endsWith('/'))
        queryString+="&recursive";
    SxQuery query(queryString, SxQuery::JOB_PUT, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, volume->nodeList()));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    if (json.object().value("requestId").toString("").isEmpty() ||
            json.object().value("minPollInterval").toInt(0) <= 0 ||
            json.object().value("maxPollInterval").toInt(0) <= 0)
    {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    return parseJobJson(json, queryResult->host(), job);
}

bool SxCluster::_getBlocks(const QList<SxBlock *> &blockList, const int blockSize)
{
    logEntry("");
    QStringList keys;
    QHash<QString, SxBlock*> hash;
    std::unique_ptr<SxQuery> query(_getBlocksMakeQuery(blockList, blockSize, keys, hash));
    if (!query)
        return false;
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(query.get(), blockList.first()->mNodeList));
    return _getBlocksProcessReply(queryResult.get(), blockSize, keys, hash);
}

bool SxCluster::_setVolumeCustomMeta(SxVolume *volume)
{
    logEntry("");
    QString queryString = QString("/%1?o=mod").arg(volume->name());

    QJsonObject jmeta;
    foreach (auto key, volume->customMeta().keys()) {
        auto value = QString::fromUtf8(volume->customMeta().value(key).toHex());
        jmeta.insert(key, value);
    }
    QJsonObject json;
    json.insert("customVolumeMeta", jmeta);
    QJsonDocument jdoc(json);
    SxQuery query(queryString, SxQuery::JOB_PUT, jdoc.toJson());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    if (!parseJson(queryResult.get(), jdoc))
        return false;
    if (jdoc.object().value("requestId").toString("").isEmpty() ||
            jdoc.object().value("minPollInterval").toInt(0) <= 0 ||
            jdoc.object().value("maxPollInterval").toInt(0) <= 0)
    {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    SxJob job;
    if (!parseJobJson(jdoc, queryResult->host(), job))
        return false;
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job))
            return false;
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    return true;
}

SxQuery *SxCluster::_getBlocksMakeQuery(const QList<SxBlock *> &blockList, const int blockSize, QStringList& keys, QHash<QString, SxBlock*>& hash)
{
    logEntry("");
    QString queryString = QString("/.data/%1/").arg(blockSize);
    if (blockList.isEmpty())
        goto invalidArgument;
    if (blockList.count()>30)
        goto invalidArgument;

    hash.clear();
    foreach (SxBlock* block, blockList) {
        if (block == nullptr || block->mHash.isEmpty())
            goto invalidArgument;
        if (hash.contains(block->mHash))
            goto invalidArgument;
        hash.insert(block->mHash, block);
    }
    keys = hash.keys();
    qSort(keys);
    foreach (QString key, keys) {
        queryString+=key;
    }
    return new SxQuery(queryString, SxQuery::GET, QByteArray());

    invalidArgument:
    mLastError = SxError(SxErrorCode::UnknownError, "invalid blocks list", QCoreApplication::translate("SxErrorMessage", "invalid blocks list"));
    logWarning(mLastError.errorMessage());
    return nullptr;
}

bool SxCluster::_getBlocksProcessReply(SxQueryResult *queryResult, const int blockSize, QStringList &keys, QHash<QString, SxBlock *> &hash)
{
    logEntry("");
    const QByteArray& data = queryResult->data();
    if (data.count() != keys.count()*blockSize) {
        logWarning(QString("wrong size: %1, expected %2").arg(data.count()).arg(keys.count() * blockSize));
        goto badReplyContent;
    }
    for (int i=0; i<keys.count(); i++) {
        QString key = keys.at(i);
        SxBlock *block = hash.value(key);
        int offset = i*blockSize;
        block->mData = data.mid(offset, blockSize);
    }
    return true;
    badReplyContent:
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

void SxCluster::setFilterInputCallback(std::function<int(sx_input_args &)> get_input)
{
    logEntry("");
    mCallbackGetInput = get_input;
}

void SxCluster::setGetLocalBlocksCallback(std::function<bool(QFile *, qint64, int, const QStringList &, QSet<QString> &)> callback)
{
    logEntry("");
    mGetLocalBlocks = callback;
}

void SxCluster::setFindIdenticalFilesCallback(std::function<bool(const QString&, qint64, int, const QStringList&, QList<QPair<QString, quint32>>&)> callback)
{
    logEntry("");
    mFindIdenticalFilesCallback = callback;
}

SxError SxCluster::lastError() const
{
    return mLastError;
}

int SxCluster::getInput(sx_input_args &args) const
{
    logEntry("");
    if (mCallbackGetInput == nullptr)
        return 1;
    return mCallbackGetInput(args);
}

bool SxCluster::reloadClusterNodes()
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logEntry("");
    if (mUseApplianceNodeList)
        return true;

    QStringList nodes;
    if (mSxAuth.initialAddress().isEmpty()) {
        QHostInfo hostInfo = QHostInfo::fromName(mSxAuth.clusterName());
        foreach (QHostAddress address, hostInfo.addresses()) {
            nodes << address.toString();
        }
    }
    else {
        nodes << mSxAuth.initialAddress();
    }

    if (_listNodes(nodes)) {
        mNodeList = nodes;
        return true;
    }
    return false;
}

bool SxCluster::reloadVolumes()
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logEntry("");
    if (!_listVolumes(mVolumeList)) {
        return false;
    }
    foreach (SxVolume* vol, mVolumeList) {
        if (!_locateVolume(vol, 0,0 ))
            return false;
    }
    return true;
}

bool SxCluster::reloadClusterMeta()
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    if (!_getUserDetails(mUserInfo))
        return false;
    if (!_getClusterMetadata(mMeta))
        return false;
    return true;
}

bool SxCluster::uploadFile(SxVolume *volume, QString path, QString localFile, SxFileEntry &fileEntry,
                           std::function<void (QString, QString, SxError, QString, quint32)> callback, bool multipart)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("start upload, volume: %1, file: %2").arg(volume->name()).arg(path));
    setAborted(false);
    QFileInfo fileInfo(localFile);
    if (!fileInfo.exists()) {
        logWarning("FILE " + localFile + " doesn't exists");
        mLastError = SxError(SxErrorCode::NotFound, "file removed before upload", QCoreApplication::translate("SxErrorMessage", "file removed before upload"));
        return false;
    }
    if (fileInfo.size() > volume->size()) {
        logWarning("FILE " + localFile + " size is bigger than volume size");
        mLastError = SxError(SxErrorCode::UnknownError, "file size is too large", QCoreApplication::translate("SxErrorMessage", "file size is too large"));
        return false;
    }
    QTemporaryFile tmpFile(fileInfo.absolutePath() + "/._sdrvtmpXXXXXX");
    QString dataPath = localFile;
    std::unique_ptr<SxFilter> filter(SxFilter::getActiveFilter(volume));
    if (filter && filter->dataPrepare()) {
        uint counter = volume->customMeta().changeCounter();
        if (!filter->dataPrepare(path, volume->customMeta(), SXF_MODE_UPLOAD)) {
            QString errorMessage;
            if (!filter->error().isEmpty())
                errorMessage = filter->error();
            else
                errorMessage = filter->lastWarning();
            mLastError = SxError(SxErrorCode::FilterError, errorMessage, QCoreApplication::translate("SxErrorMessage", errorMessage.toUtf8().constData()));
            logWarning(mLastError.errorMessage());
            return false;
        }
        if (filter->dataProcess()) {
            QFile inputFile(localFile);
            if (!inputFile.open(QIODevice::ReadOnly)) {
                logWarning("unable to open file" + inputFile.fileName());
                return false;
            }
            XFile::makeInvisible(tmpFile.fileName(), true);
            if (!tmpFile.open()) {
                logWarning("unable to open file" + tmpFile.fileName());
                return false;
            }
            if (!_filter_data_process(&inputFile, &tmpFile, filter.get(), path, false)) {
                logWarning("data process failed");
                return false;
            }
            inputFile.close();
            tmpFile.close();
            dataPath = tmpFile.fileName();
            if (callback)
                pollUploadJobs(sUploadJobsLimit, callback);
        }
        if (counter != volume->customMeta().changeCounter()) {
            if (!_setVolumeCustomMeta(volume)) {
                return false;
            }
        }
    }

    int blockSize = 0;
    if (!_locateVolume(volume, QFile(dataPath).size(), &blockSize))
        return false;

    fileInfo.refresh();
    uint mtime = fileInfo.lastModified().toTime_t();
    SxFile file(volume, path, mClusterUuid, dataPath, blockSize, QFile(localFile).size(), [this]()->bool {return aborted();}, multipart);
    if (aborted())
        return false;
    if (!file.multipart()) {
        SxFile remoteFile(volume, path, "", true);
        if (_getFile(remoteFile, true)) {
            if (file.haveEqualContent(remoteFile)) {
                fileEntry.mPath = path;
                fileEntry.mRevision = remoteFile.mRevision;
                fileEntry.mCreatedAt = fileInfo.lastModified().toTime_t();
                fileEntry.mSize = remoteFile.mRemoteSize;
                fileEntry.mBlockSize = remoteFile.mBlockSize;
                fileEntry.mBlocks.clear();
                foreach(auto block, remoteFile.mBlocks) {
                    fileEntry.mBlocks.append(block->mHash);
                }
                logVerbose(QString("skiping upload of file %1").arg(path));
                return true;
            }
        }
    }
    fileInfo.refresh();
    if (!fileInfo.exists()) {
        mLastError = SxError(SxErrorCode::NotFound, "file removed before upload", QCoreApplication::translate("SxErrorMessage", "file removed before upload"));
        return false;
    }
    if (mtime != fileInfo.lastModified().toTime_t()) {
        mLastError = SxError(SxErrorCode::SoftError, "file changed before upload", QCoreApplication::translate("SxErrorMessage", "file changed before upload"));
        return false;
    }
    if (!_initializeFile(file)) {
        fileEntry.mSize = file.mRemoteSize;
        return false;
    }
    if (callback)
        pollUploadJobs(sUploadJobsLimit, callback);
    qint64 uploadSize;
    qint64 uploadSkipped = 0;
    qint64 chunkSize = 0;
    if (file.multipart()) {
        qint64 blockCount = file.mRemoteSize / blockSize;
        if (file.mRemoteSize % blockSize)
            ++blockCount;
        uploadSize = blockCount*blockSize;
        chunkSize = static_cast<qint64>(file.mBlocks.size())*blockSize;
    }
    else
        uploadSize = static_cast<qint64>(file.mBlocksToSend.size())*blockSize;
    qint64 uploaded = 0;
    QDateTime uploadStart = QDateTime::currentDateTime();

    sendChunk:
    if (file.multipart()) {
        uploadSkipped+=(chunkSize-static_cast<qint64>(file.mBlocksToSend.size())*blockSize);
    }
    if (!file.mBlocksToSend.isEmpty()) {
        auto offsets = file.getBlocksOffsets();
        auto toSent = QList<SxBlock*>(file.mBlocksToSend);

        while (!toSent.isEmpty()) {
            static const int dataLimit = 4*1024*1024;

            fileInfo.refresh();
            if (!fileInfo.exists()) {
                mLastError = SxError(SxErrorCode::NotFound, "file removed before upload", QCoreApplication::translate("SxErrorMessage", "file removed before upload"));
                return false;
            }
            if (mtime != fileInfo.lastModified().toTime_t()) {
                mLastError = SxError(SxErrorCode::SoftError, "file changed before upload", QCoreApplication::translate("SxErrorMessage", "file changed before upload"));
                return false;
            }

            QList<SxBlock*> chunk;
            chunk.append(toSent.takeFirst());
            QSet<QString> targetNodes = chunk.first()->mNodeList.toSet();

            foreach (SxBlock *block, toSent) {
                if (chunk.size()*blockSize >= dataLimit)
                    break;
                QSet<QString> intersection = block->mNodeList.toSet() & targetNodes;
                if (intersection.isEmpty())
                    continue;
                chunk.append(block);
                toSent.removeOne(block);
                targetNodes = intersection;
            }
            qint64 dataLen = chunk.size()*blockSize;
            char* rawData = new char[dataLen];
            qint64 dataOffset = 0;

            foreach (SxBlock *block, chunk) {
                qint64 offset = offsets.value(block).first();
                if (!file.readBlock(offset, blockSize, rawData+dataOffset))
                {
                    logWarning("reading block " + block->mHash + " failed");
                    delete [] rawData;
                    return false;
                }
                dataOffset+=blockSize;
            }
            QStringList target = {targetNodes.toList().first()};
            const QByteArray data = QByteArray::fromRawData(rawData, static_cast<int>(dataLen));
            if (!_createBlocks(file.mUploadToken, blockSize, data, target)) {
                if (mLastError.errorCode() == SxErrorCode::AbortedByUser) {
                    delete [] rawData;
                    return false;
                }
                foreach (SxBlock *block, chunk) {
                    foreach (QString node, targetNodes) {
                        block->mNodeList.removeOne(node);
                    }
                    if (block->mNodeList.isEmpty()) {
                        delete [] rawData;
                        return false;
                    }
                    toSent.append(block);
                }
            }
            else {
                uploaded += dataLen;
                double uploadTime = uploadStart.msecsTo(QDateTime::currentDateTime())/1000.0;
                if (uploadTime > 0) {
                    qint64 speed = static_cast<qint64>(uploaded / uploadTime);
                    qint64 size = uploadSize - uploaded - uploadSkipped;
                    emit sig_setProgress(size, speed);
                }
            }
            if (callback)
                pollUploadJobs(sUploadJobsLimit, callback);
            delete [] rawData;
        }
    }
    double uploadTime = uploadStart.msecsTo(QDateTime::currentDateTime())/1000.0;
    if (uploadTime > 0) {
        qint64 speed = static_cast<qint64>(uploaded / uploadTime);
        qint64 size = uploadSize - uploaded - uploadSkipped;
        emit sig_setProgress(size, speed);
    }

    if (file.multipart() && file.canReadNextChunk()) {
        int blockCount = file.mBlocks.count();
        if (!file.readNextChunk()) {
            mLastError = SxError(SxErrorCode::IOError, "unable to read file", "unable to read file");
            return false;
        }
        chunkSize = (file.mBlocks.count() - blockCount)*file.mBlockSize;
        if (!_initializeFileAddChunk(file, blockCount)) {
            fileEntry.mSize = file.mRemoteSize;
            return false;
        }
        goto sendChunk;
    }

    std::unique_ptr<SxJob> job(new SxJob());
    if (!_flushFile(file, *job)) {
        return false;
    }
    fileEntry.mPath = path;
    fileEntry.mCreatedAt = 0;
    fileEntry.mSize = 0;
    fileEntry.mRevision = "";
    fileEntry.mBlockSize = file.mBlockSize;
    fileEntry.mBlocks.clear();
    foreach(auto block, file.mBlocks) {
        fileEntry.mBlocks.append(block->mHash);
    }

    if (callback == nullptr) {
        while (job->mStatus == SxJob::PENDING) {
            job->waitInterval();
            if (!_poll(*job))
                return false;
        }
        if (job->mStatus == SxJob::ERROR)
            return false;

        SxFile test(volume, path, "", true);
        if (_getFile(test)) {
            if (file.haveEqualContent(test)) {
                fileEntry.mRevision = test.mRevision;
            }
            fileEntry.mCreatedAt = fileInfo.lastModified().toTime_t();
            fileEntry.mSize = test.mRemoteSize;
        }
        logVerbose("upload done");
        return true;
    }
    else {
        QStringList blocks;
        foreach (SxBlock* block, file.mBlocks) {
            blocks.append(block->mHash);
        }
        mUploadJobs.insert(job.release(), {volume, path, fileInfo.lastModified().toTime_t(), QDateTime::currentDateTime(), file.mRemoteSize, blocks});
        pollUploadJobs(sUploadJobsLimit, callback);
        return true;
    }
}

bool SxCluster::createEmptyFile(SxVolume *volume, QString path)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("start upload, volume: %1, file: %2").arg(volume->name()).arg(path));
    setAborted(false);

    SxFile file(volume, path, "", true);
    if (!_initializeFile(file)) {
        return false;
    }
    std::unique_ptr<SxJob> job(new SxJob());
    if (!_flushFile(file, *job)) {
        return false;
    }
    while (job->mStatus == SxJob::PENDING) {
        job->waitInterval();
        if (!_poll(*job))
            return false;
    }
    if (job->mStatus == SxJob::ERROR)
        return false;
    return true;
}

QString radomValueFromList(QStringList &list) {
    if (list.isEmpty())
        return QString::null;
    if (list.count()==1)
        return list.first();
    int random_value = qrand();
    return list.value(random_value%list.count());
}

bool SxCluster::listFileRevisions(SxVolume *volume, const QString &path, QList<std::tuple<QString, qint64, quint32>> &list)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("volume: %1, file: %2").arg(volume->name()).arg(path));
    SxFile file(volume, path, "", true);
    if(!testFile(file))
        return false;
    QString queryString = "/"+file.mVolume->name();
    if (file.mRemotePath.startsWith("/"))
        queryString += QUrl::toPercentEncoding(file.mRemotePath, "/");
    else
        queryString += "/"+QUrl::toPercentEncoding(file.mRemotePath, "/");
    queryString+="?fileRevisions";
    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, volume->nodeList()));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    auto jRevisions = json.object().value("fileRevisions");
    if (!jRevisions.isObject())
        goto badReplyContent;
    foreach (QString key, jRevisions.toObject().keys()) {
        auto jRev = jRevisions.toObject().value(key).toObject();
        if (!jRev.value("blockSize").isDouble() || !jRev.value("fileSize").isDouble() || !jRev.value("createdAt").isDouble())
            goto badReplyContent;
    }
    list.clear();
    foreach (QString key, jRevisions.toObject().keys()) {
        auto jRev = jRevisions.toObject().value(key).toObject();
        list.append(std::make_tuple(key, jRev.value("fileSize").toVariant().toLongLong(), jRev.value("createdAt").toVariant().toUInt()));
    }
    return true;
    badReplyContent:
    file.clearBlocks();
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::getFileRevision(SxVolume *volume, const QString &path, const QString &node, QString &revision)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    SxFile file(volume, path, "", true);
    if(!testFile(file))
        return false;

    QString remotePath = file.mRemotePath;
    if (file.mRemotePath.startsWith("/"))
        remotePath = file.mRemotePath.mid(1);
    else
        remotePath = file.mRemotePath;

    QString queryString = "/"+file.mVolume->name()+"?o=list&filter="+QUrl::toPercentEncoding(remotePath, "/");

    SxQuery query(queryString, SxQuery::GET, QByteArray());
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, {node}));
    if (!queryResult)
        return false;
    QJsonDocument json;
    QJsonObject jList;
    if (!parseJson(queryResult.get(), json))
        return false;
    if (!json.object().value("fileList").isObject())
        goto badReplyContent;
    jList = json.object().value("fileList").toObject();
    revision.clear();

    for (auto iterator = jList.begin(); iterator != jList.end(); iterator++)
    {
        QString path = iterator.key();
        if (path.endsWith("/"))
            continue;
        auto jFile = iterator.value().toObject();
        auto jRev = jFile.value("fileRevision");
        if (jRev.isString()) {
            revision = jRev.toString();
            break;
        }
    }

    return true;
    badReplyContent:
    file.clearBlocks();
    mLastError = SxError::errorBadReplyContent();
    logWarning(mLastError.errorMessage());
    return false;
}

bool SxCluster::restoreFileRevision(SxVolume *volume, const QString &source, const QString &rev, const QString &destination)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("volume: %1, source: %2, rev: %3, destination: %4").arg(volume->name()).arg(source).arg(rev).arg(destination));
    SxFile file(volume, source, rev, true);
    if(!testFile(file))
        return false;
    if (!_getFile(file))
        return false;

    SxFile dest(volume, destination, "", true);
    QStringList blocks;
    foreach (auto b, file.mBlocks) {
        blocks.append(b->mHash);
    }
    dest.fakeFile(file.mRemoteSize, blocks, file.mBlockSize);
    if (!_initializeFile(dest))
        return false;
    /*
    if (!dest.mBlocksToSend.isEmpty()) {
        mLastErrorMessage = "some blocks are missing";
        return false;
    }
    */
    SxJob job;
    if (!_flushFile(dest, job))
        return false;
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job))
            return false;
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    return true;
}

bool SxCluster::copyFile(SxVolume *srcVolume, const QString &source, SxVolume *dstVolume, const QString &destination)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    if (srcVolume == nullptr || dstVolume == nullptr || source.isEmpty() || destination.isEmpty())
        return false;
    SxFile file(srcVolume, source, "", true);
    if(!testFile(file))
        return false;
    if (!_getFile(file))
        return false;

    SxFile dest(dstVolume, destination, "", true);
    QStringList blocks;
    foreach (auto b, file.mBlocks) {
        blocks.append(b->mHash);
    }
    dest.fakeFile(file.mRemoteSize, blocks, file.mBlockSize);
    if (!_initializeFile(dest))
        return false;
    SxJob job;
    if (!_flushFile(dest, job))
        return false;
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job))
            return false;
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    return true;
}

bool SxCluster::copyFiles(SxVolume *srcVolume, const QString &srcDir, const QStringList &files, SxVolume *dstVolume, const QString &dstDir, std::function<void(int, int)> progressCallback)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    if (srcVolume == nullptr || dstVolume == nullptr || srcDir.isEmpty() || dstDir.isEmpty() || files.isEmpty())
        return false;
    QList<SxJob> jobs;
    int done = 0;
    progressCallback(done, files.size());
    foreach (auto f, files) {
        QString source = srcDir+f;
        QString destination = dstDir+f;
        SxFile file(srcVolume, source, "", true);
        if(!testFile(file))
            return false;
        if (!_getFile(file))
            return false;
        SxFile dest(dstVolume, destination, "", true);
        QStringList blocks;
        foreach (auto b, file.mBlocks) {
            blocks.append(b->mHash);
        }
        dest.fakeFile(file.mRemoteSize, blocks, file.mBlockSize);
        if (!_initializeFile(dest))
            return false;
        SxJob job;
        if (!_flushFile(dest, job))
            return false;
        jobs.append(job);
        while (jobs.size() > 30) {
            jobs.first().waitInterval();
            auto it = jobs.begin();
            while (it != jobs.end()) {
                if (!_poll(*it))
                    return false;
                if (it->mStatus == SxJob::ERROR)
                    return false;
                if (it->mStatus == SxJob::OK) {
                    it = jobs.erase(it);
                    ++done;
                    progressCallback(done, files.size());
                }
                else
                    ++it;
            }
        }
        auto it = jobs.begin();
        while (it != jobs.end()) {
            if (!_poll(*it))
                return false;
            if (it->mStatus == SxJob::ERROR)
                return false;
            if (it->mStatus == SxJob::OK) {
                it = jobs.erase(it);
                ++done;
                progressCallback(done, files.size());
            }
            else
                ++it;
        }
    }
    while (!jobs.isEmpty()) {
        jobs.first().waitInterval();
        auto it = jobs.begin();
        while (it != jobs.end()) {
            if (!_poll(*it))
                return false;
            if (it->mStatus == SxJob::ERROR)
                return false;
            if (it->mStatus == SxJob::OK) {
                it = jobs.erase(it);
                ++done;
                progressCallback(done, files.size());
            }
            else
                ++it;
        }
    }
    return true;
}

bool SxCluster::moveFiles(SxVolume *srcVolume, const QString &srcDir, const QStringList &files, SxVolume *dstVolume, const QString &dstDir, std::function<void (int, int)> progressCallback)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    if (srcVolume == nullptr || dstVolume == nullptr || srcDir.isEmpty() || dstDir.isEmpty() || files.isEmpty())
        return false;
    QList<SxJob> jobs;
    int done = 0;
    progressCallback(done, files.size());
    foreach (auto f, files) {
        QString source = srcDir+f;
        QString destination = dstDir+f;
        SxFile file(srcVolume, source, "", true);
        if(!testFile(file))
            return false;
        if (!_getFile(file))
            return false;
        SxFile dest(dstVolume, destination, "", true);
        QStringList blocks;
        foreach (auto b, file.mBlocks) {
            blocks.append(b->mHash);
        }
        dest.fakeFile(file.mRemoteSize, blocks, file.mBlockSize);
        if (!_initializeFile(dest))
            return false;
        SxJob jobFlush, jobDelete;
        if (!_flushFile(dest, jobFlush))
            return false;
        jobs.append(jobFlush);
        if (!_deleteFile(file, jobDelete))
            return false;
        jobs.append(jobDelete);

        while (jobs.size() > 30) {
            jobs.first().waitInterval();
            auto it = jobs.begin();
            while (it != jobs.end()) {
                if (!_poll(*it))
                    return false;
                if (it->mStatus == SxJob::ERROR)
                    return false;
                if (it->mStatus == SxJob::OK) {
                    it = jobs.erase(it);
                    ++done;
                    progressCallback(done, 2*files.size());
                }
                else
                    ++it;
            }
        }
        auto it = jobs.begin();
        while (it != jobs.end()) {
            if (!_poll(*it))
                return false;
            if (it->mStatus == SxJob::ERROR)
                return false;
            if (it->mStatus == SxJob::OK) {
                it = jobs.erase(it);
                ++done;
                progressCallback(done, 2*files.size());
            }
            else
                ++it;
        }
    }
    while (!jobs.isEmpty()) {
        jobs.first().waitInterval();
        auto it = jobs.begin();
        while (it != jobs.end()) {
            if (!_poll(*it))
                return false;
            if (it->mStatus == SxJob::ERROR)
                return false;
            if (it->mStatus == SxJob::OK) {
                it = jobs.erase(it);
                ++done;
                progressCallback(done, 2*files.size());
            }
            else
                ++it;
        }
    }
    return true;
}

bool SxCluster::uploadFiles(const QString &localDir, const QStringList &files, SxVolume *dstVolume, const QString &dstDir, std::function<void (QString, qint64, qint64)> progressCallback, bool multipart)
{
    setAborted(false);
    if (dstVolume == nullptr)
        return false;
    auto callback = [this](QString volume, QString file, SxError error, QString rev, quint32 mtime) {
        Q_UNUSED(volume);
        Q_UNUSED(file);
        Q_UNUSED(error);
        Q_UNUSED(rev);
        Q_UNUSED(mtime);
    };

    qint64 done = 0;
    qint64 currentFileSize = 0;
    QString currentFile;

    auto setProgress = [&currentFile, &done, &currentFileSize, progressCallback](qint64 size, qint64) {
        if (size >= currentFileSize)
            return;
        qint64 progress = currentFileSize-size;
        currentFileSize = size;
        done+=progress;
        progressCallback(currentFile, done, 0);
    };
    QMetaObject::Connection connection = connect(this, &SxCluster::sig_setProgress, setProgress);

    foreach (QString file, files) {
        if (aborted())
            return false;
        currentFile = file;
        QFileInfo fileInfo(currentFile);
        currentFileSize = fileInfo.size();
        progressCallback(currentFile, done, 0);
        QString remotePath = dstDir+file.mid(localDir.length()+1);
        QTemporaryFile tmpFile;
        if (file.endsWith("/.sxnewdir")) {
            QFile sxnewdir(file);
            if (!sxnewdir.exists()) {
                tmpFile.open();
                file = tmpFile.fileName();
            }
        }
        SxFileEntry fileEntry;
        if (!uploadFile(dstVolume, remotePath, file, fileEntry, callback, multipart)) {
            return false;
        }
        if (currentFileSize>0) {
            done+=currentFileSize;
            progressCallback(currentFile, done, 0);
        }
    }
    progressCallback(currentFile, done, 0);
    disconnect(connection);
    pollUploadJobs(0, callback);
    return true;
}

int SxCluster::uploadJobsCount() const
{
    QMutexLocker locker(&mUploadJobMutex);
    return mUploadJobs.count();
}

bool SxCluster::pollUploadJobs(int jobLimit, std::function<void (QString, QString, SxError, QString, quint32)> callback)
{
    QMutexLocker locker(&mUploadJobMutex);
    if (mUploadJobs.isEmpty())
        return true;
    do {
        int counter = 0;
        foreach (auto job, mUploadJobs.keys()) {
            auto jobInfo = mUploadJobs[job];
            auto timeElapsed = jobInfo.lastPollTime.msecsTo(QDateTime::currentDateTime());
            if (timeElapsed > job->mInterval) {
                ++counter;
                if (job->mInterval < job->mMaxPollInterval)
                    job->mInterval+=100;
                if (_poll(*job)) {
                    if (job->mStatus == SxJob::PENDING) {
                        jobInfo.lastPollTime = QDateTime::currentDateTime();
                    }
                    else if (job->mStatus == SxJob::ERROR) {
                        callback(jobInfo.volume->name(), jobInfo.path, mLastError, "", jobInfo.mTime);
                        mUploadJobs.remove(job);
                        delete job;
                    }
                    else {
                        QString rev;
                        SxFile test(jobInfo.volume, jobInfo.path, "", true);
                        if (_getFile(test)) {
                            if (test.haveEqualContent(test)) {
                                rev = test.mRevision;
                            }
                        }
                        callback(jobInfo.volume->name(), jobInfo.path, SxError(SxErrorCode::NoError, "", ""), rev, jobInfo.mTime);
                        mUploadJobs.remove(job);
                        delete job;
                    }
                }
            }
        }
        if (counter == 0)
            mUploadJobs.keys().first()->waitInterval();
    } while (mUploadJobs.count() > jobLimit);
    return true;
}

void SxCluster::clearUploadJobs()
{
    QMutexLocker locker(&mUploadJobMutex);
    foreach (auto job, mUploadJobs.keys()) {
        delete job;
    }
    mUploadJobs.clear();
}

bool SxCluster::downloadFile(SxVolume *volume, QString path, QString localFilePath, SxFileEntry &fileEntry, int connectionLimit)
{
    return downloadFile(volume, path, "", localFilePath, fileEntry, connectionLimit);
}

bool SxCluster::downloadFile(SxVolume *volume, QString path, QString rev, QString localFilePath, SxFileEntry &fileEntry, int connectionLimit)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("volume: %1, file: %2").arg(volume->name()).arg(path));
    setAborted(false);
    mLastError = SxError();
    qsrand(QDateTime::currentDateTime().toTime_t());
    SxFile file(volume, path, rev, true);
    if (!_getFile(file)) {
        logWarning(mLastError.errorMessage());
        return false;
    }
    emit sig_setDownloadSize(file.remoteSize());
    QString tmpName;
    QDateTime start;
    qint64 downloaded, downloadSize;
    QHash<SxQuery*, QPair<QStringList*, QHash<QString, SxBlock*>* > > activeQueriesHelper;
    QHash<SxQuery*, QStringList*> activeQueries;

    if (connectionLimit == 0 && file.mBlocks.size()>0) {
        connectionLimit = file.mBlocks.first()->mNodeList.count();
        if (connectionLimit < 2)
            connectionLimit = 2;
    }
    std::unique_ptr<SxFilter> filter(SxFilter::getActiveFilter(volume));
    if (filter && filter->dataPrepare()) {
        if (!filter->dataPrepare(file.mLocalPath, volume->customMeta(), SXF_MODE_DOWNLOAD)) {
            mLastError = SxError(SxErrorCode::FilterError, filter->lastWarning(), QCoreApplication::translate("SxErrorMessage", filter->lastWarning().toUtf8().constData()));
            return false;
        }
    }

    QFileInfo localFileInfo(localFilePath);
    if (localFileInfo.isDir()) {
        mLastError = SxError(SxErrorCode::FileDirConflict, "directory/file conflict", tr("directory/file conflict"));
        return false;
    }
    QDateTime mtime;
    QDir parentDir(localFileInfo.absolutePath());
    if (!parentDir.exists() && !parentDir.mkpath(".")) {
        QString path = parentDir.absolutePath();
        while (path.contains("/")) {
            int index = path.lastIndexOf("/");
            path = path.mid(0, index);
            QFile conflictedFile(path);
            if (conflictedFile.exists()) {
                mLastError = SxError(SxErrorCode::FileDirConflict,
                                     QString("file %1 prevents to create directory %2").arg(path).arg(parentDir.absolutePath()),
                                     tr("file %1 prevents to create directory %2").arg(path).arg(parentDir.absolutePath()));
                return false;
            }
        }
        mLastError = SxError(SxErrorCode::IOError,
                             QString("unable to create directory %1").arg(parentDir.absolutePath()),
                             tr("unable to create directory %1").arg(parentDir.absolutePath()));
        return false;
    }

    std::unique_ptr<QTemporaryFile> tmpFile(new QTemporaryFile(parentDir.absolutePath() + "/._sdrvtmpXXXXXX"));
    if (!tmpFile->open()) {
        mLastError = SxError(SxErrorCode::IOError, "unable to open tempfile", QCoreApplication::translate("SxErrorMessage", "unable to open tempfile"));
        logWarning("unable to open file" + localFilePath);
        return false;
    }
    XFile::makeInvisible(tmpFile->fileName(), true);
    QHash<QString, int> mNodeQuerriesCounter;
    foreach (QString node, volume->nodeList()) {
        mNodeQuerriesCounter.insert(node, 0);
    }

#ifdef Q_OS_LINUX
    if (file.mRemoteSize > 0 && fallocate(tmpFile->handle(), 0, 0, file.mRemoteSize) < 0) {
#else
    if (!tmpFile->resize(file.mRemoteSize)) {
#endif
        mLastError = SxError(SxErrorCode::IOError, "unable to resize file", QCoreApplication::translate("SxErrorMessage", "unable to resize file"));
        logWarning("unable to resize file" + localFilePath);
        return false;
    }
    auto blocksOffsets = file.getBlocksOffsets();
    auto toDownload = blocksOffsets.keys();

    const int batchLimit = 4*1024*1024;
    int blocksLimit = 30;
    if (blocksLimit * file.mBlockSize > batchLimit) {
        blocksLimit = batchLimit/file.mBlockSize;
    }

    if (!filter || !filter->dataProcess()) {
        if (mGetLocalBlocks && file.mRemoteSize > 0) {
            QStringList blockList;
            QSet<QString> missingBlocks;
            foreach (auto block, file.mBlocks) {
                blockList.append(block->mHash);
            }
            if (!mGetLocalBlocks(tmpFile.get(), file.mRemoteSize, file.mBlockSize, blockList, missingBlocks)) {
                mLastError = SxError(SxErrorCode::IOError, "creating tempFile failed", "creating tempFile failed");
                return false;
            }
            foreach (auto block, toDownload) {
                if (!missingBlocks.contains(block->mHash)) {
                    toDownload.removeOne(block);
                }
            }
        }
        else if (localFileInfo.exists())
        {
            mtime = localFileInfo.lastModified();
            auto lambda = [&file, &toDownload, &blocksOffsets, &tmpFile, this](const QString& hash, const QByteArray& data) -> bool {
                if (file.mUniqueBlocks.contains(hash)) {
                    SxBlock* block = file.mUniqueBlocks.value(hash);
                    if (toDownload.contains(block)) {
                       auto list = blocksOffsets.value(block);
                       foreach (qint64 offset, list) {
                           if (!tmpFile->seek(offset)) {
                               mLastError = SxError(SxErrorCode::IOError, tmpFile->errorString(), tmpFile->errorString());
                               return false;
                           }
                           qint64 toWrite = file.mBlockSize;
                           if (tmpFile->size()-offset < toWrite)
                               toWrite = tmpFile->size()-offset;
                           if (tmpFile->write(data.constData(), toWrite) != toWrite) {
                               mLastError = SxError(SxErrorCode::IOError, tmpFile->errorString(), tmpFile->errorString());
                               return false;
                           }
                       }
                       toDownload.removeOne(block);
                    }
                }
                return true;
            };

            QFile oldFile(localFileInfo.absoluteFilePath());
            if (oldFile.open(QIODevice::ReadOnly)) {
                int blockSize = file.mBlockSize;
                qint64 offset = 0;
                qint64 oldFileSize = oldFile.size();
                QByteArray blockData;
                bool failed = false;
                while(offset + blockSize <= oldFileSize) {
                    if (QCoreApplication::instance()->thread() == QThread::currentThread()){
                        QEventLoop loop;
                        loop.processEvents(QEventLoop::AllEvents, 10);
                    }
                    if (aborted()) {
                        return false;
                    }
                    offset+=blockSize;
                    blockData = oldFile.read(blockSize);
                    if (blockData.size() != blockSize) {
                        failed = true;
                        break;
                    }
                    QString hash = QString::fromUtf8(SxBlock::hashBlock(blockData,mClusterUuid));
                    if (!lambda(hash, blockData)) {
                        failed = true;
                        break;
                    }
                }
                if (oldFileSize - offset > 0 && !failed) {
                    int size = static_cast<int>(oldFileSize - offset);
                    blockData = oldFile.read(size);
                    if (blockData.size() == size) {
                        blockData.resize(blockSize);
                        for (int i=size; i<blockSize; i++) {
                            blockData[i]=0;
                        }
                        QString hash = QString::fromUtf8(SxBlock::hashBlock(blockData,mClusterUuid));
                        lambda(hash, blockData);
                    }
                }
            }
        }
    }
    else if (mFindIdenticalFilesCallback && file.mRemoteSize > 0) {
        QStringList blockList;
        QList<QPair<QString, quint32>> files;
        foreach (auto block, file.mBlocks) {
            blockList.append(block->mHash);
        }
        if (mFindIdenticalFilesCallback(volume->name(), file.mRemoteSize, file.mBlockSize, blockList, files)) {
            bool resized = false;
            foreach (auto pair, files) {
                QFileInfo fileInfo(pair.first);
                if (fileInfo.exists() && fileInfo.lastModified().toTime_t() == pair.second) {
                    qint64 size = fileInfo.size();
                    tmpFile->resize(0);
                    #ifdef Q_OS_LINUX
                    if (fallocate(tmpFile->handle(), 0, 0, size) < 0) {
                    #else
                    if (!tmpFile->resize(size)) {
                    #endif
                        mLastError = SxError(SxErrorCode::IOError, "unable to resize file", QCoreApplication::translate("SxErrorMessage", "unable to resize file"));
                        logWarning("unable to resize file" + localFilePath);
                        return false;
                    }
                    resized = true;
                    tmpFile->seek(0);
                    QFile in_file(pair.first);
                    if (!in_file.open(QIODevice::ReadOnly)) {
                        mLastError = SxError(SxErrorCode::IOError, "unable to open file", QCoreApplication::translate("SxErrorMessage", "unable to open file"));
                        return false;
                    }
                    qint64 offset = 0;
                    int bufforSize = 1024*1024;
                    while (true) {
                        qint64 toRead = bufforSize;
                        if (offset + bufforSize > size)
                            toRead = size - offset;
                        QByteArray data = in_file.read(toRead);
                        if (data.size() != toRead) {
                            mLastError = SxError(SxErrorCode::IOError, in_file.errorString(), in_file.errorString());
                            return false;
                        }
                        if (tmpFile->write(data) != toRead) {
                            mLastError = SxError(SxErrorCode::IOError, in_file.errorString(), in_file.errorString());
                            return false;
                        }
                        offset += bufforSize;
                        if (offset >= size)
                            break;
                    }
                    fileInfo.refresh();
                    if (fileInfo.lastModified().toTime_t() != pair.second)
                        continue;
                    if (!tmpFile->flush()) {
                        mLastError = SxError(SxErrorCode::IOError, tmpFile->errorString(), tmpFile->errorString());
                        return false;
                    }
                    tmpFile->setAutoRemove(false);
                    tmpFile->close();
                    tmpName = tmpFile->fileName();
                    goto renameFile;
                }
            }
            if (resized) {
                tmpFile->resize(0);
                #ifdef Q_OS_LINUX
                if (fallocate(tmpFile->handle(), 0, 0, file.mRemoteSize) < 0) {
                #else
                if (!tmpFile->resize(file.mRemoteSize)) {
                #endif
                    mLastError = SxError(SxErrorCode::IOError, "unable to resize file", QCoreApplication::translate("SxErrorMessage", "unable to resize file"));
                    logWarning("unable to resize file" + localFilePath);
                    return false;
                }
            }
        }
    }
    {
        downloaded = 0;
        downloadSize = static_cast<qint64>(toDownload.size())*file.mBlockSize;
        start = QDateTime::currentDateTime();

        while (!toDownload.isEmpty() || !activeQueries.isEmpty()) {
            if (mtime.isValid()) {
                localFileInfo.refresh();
                if (mtime != localFileInfo.lastModified()) {
                    logWarning(QString("file %1 changed during download").arg(localFileInfo.absoluteFilePath()));
                    mLastError = SxError(SxErrorCode::UnknownError, "file changed during download", QCoreApplication::translate("SxErrorMessage", "file changed during download"));
                    abortAllQueries();
                    return false;
                }
            }
            while (activeQueriesHelper.count() < connectionLimit) {
                QList<SxBlock*> batch;
                if (toDownload.isEmpty())
                    break;
                batch.append(toDownload.takeFirst());
                QString target = radomValueFromList(batch.first()->mNodeList);
                QSet<QString> nodeCounter;
                foreach (auto node, batch.first()->mNodeList) {
                    nodeCounter.insert(node);
                }

                foreach (SxBlock* block, toDownload) {
                    if (batch.count() >= blocksLimit)
                        break;
                    if (block->mNodeList.contains(target)) {
                        batch.append(block);
                        toDownload.removeAll(block);
                        foreach (auto node, nodeCounter) {
                            if (!block->mNodeList.contains(node))
                                nodeCounter.remove(node);
                        }
                    }
                }

                QStringList *keys = new QStringList();
                QHash<QString, SxBlock*> *hashMap = new QHash<QString, SxBlock*>();
                SxQuery* query = _getBlocksMakeQuery(batch, file.mBlockSize, *keys, *hashMap);
                QStringList *targets = new QStringList(nodeCounter.toList());

                foreach (SxBlock* block, batch) {
                    foreach (QString node, *targets) {
                        block->mNodeList.removeOne(node);
                    }
                }
                activeQueries.insert(query, targets);
                activeQueriesHelper.insert(query, {keys, hashMap});
            }

            auto selectResult = querySelect(activeQueries);

            SxQuery* currentQuerry = selectResult.first;
            std::unique_ptr<SxQueryResult> queryResult(selectResult.second);
            if (!queryResult)
                return false;
            auto queryHelper = activeQueriesHelper.value(currentQuerry);
            QStringList *keys = queryHelper.first;
            QHash<QString, SxBlock*> *hashMap = queryHelper.second;

            ++mNodeQuerriesCounter[queryResult->host()];
            if (queryResult->error().errorCode() == SxErrorCode::Timeout || queryResult->error().errorCode() == SxErrorCode::SslError) {
                logWarning(queryResult->error().errorMessage());
                if (connectionLimit > 1)
                    --connectionLimit;
                foreach (SxBlock* block, hashMap->values()) {
                    if (block->mNodeList.isEmpty()) {
                        logWarning("failed to get block " + block->mHash + " (all nodes failed)");
                        tmpFile->close();
                        tmpFile->remove();
                        goto cleanMemory;
                    }
                    toDownload.append(block);
                }
            }
            else if (queryResult->error().errorCode() != SxErrorCode::NoError) {
                if (queryResult->error().errorCode() == SxErrorCode::AbortedByUser) {
                    mLastError = queryResult->error();
                    logVerbose(mLastError.errorMessage());
                }
                else {
                    logWarning("download failed:" + queryResult->error().errorMessage());
                    QJsonDocument jDoc;
                    parseJson(queryResult.get(), jDoc);
                }
                tmpFile->close();
                tmpFile->remove();
                goto cleanMemory;
            }
            else {
                if (!_getBlocksProcessReply(queryResult.get(), file.mBlockSize, *keys, *hashMap)) {
                    tmpFile->close();
                    tmpFile->remove();
                    logWarning("failed to get blocks");
                    goto cleanMemory;
                }

                foreach (SxBlock* block, hashMap->values()) {
                    auto offsetList = blocksOffsets.value(block);
                    foreach (auto offset, offsetList) {
                        if (QCoreApplication::instance()->thread() == QThread::currentThread()){
                            QEventLoop loop;
                            loop.processEvents(QEventLoop::AllEvents, 10);
                        }
                        if (aborted()) {
                            return false;
                        }
                        if (!tmpFile->seek(offset)) {
                            mLastError = SxError(SxErrorCode::IOError, tmpFile->errorString(), tmpFile->errorString());
                            goto io_error;
                        }

                        qint64 toWrite = file.mBlockSize;
                        if (tmpFile->size()-offset < file.mBlockSize)
                            toWrite = tmpFile->size()-offset;
                        if (tmpFile->write(block->mData.constData(), toWrite) != toWrite) {
                            mLastError = SxError(SxErrorCode::IOError, tmpFile->errorString(), tmpFile->errorString());
                            goto io_error;
                        }
                    }
                    block->mData.clear();
                }

                downloaded += static_cast<qint64>(keys->count())*file.mBlockSize;
                double downloadTime = start.msecsTo(QDateTime::currentDateTime())/1000.0;
                qint64 speed = 0;
                if (downloadTime>0) {
                    qint64 size = downloadSize - downloaded;
                    speed = static_cast<qint64>(downloaded/downloadTime);
                    emit sig_setProgress(size, speed);
                }
            }
            activeQueriesHelper.remove(currentQuerry);
            delete keys;
            delete hashMap;
            QStringList* targets = activeQueries.take(currentQuerry);
            if (targets)
                delete targets;
            delete currentQuerry;
        }
        if (!tmpFile->flush()) {
            mLastError = SxError(SxErrorCode::IOError, tmpFile->errorString(), tmpFile->errorString());
            goto io_error;
        }
        tmpFile->close();
    }
    {
        double downloadTime = start.msecsTo(QDateTime::currentDateTime())/1000.0;
        qint64 speed = 0;
        if (downloadTime>0)
            speed = static_cast<qint64>(downloaded/downloadTime);
        logVerbose(QString("download time: %1, speed %2").arg(downloadTime).arg(speed));
    }

    foreach (QString key, mNodeQuerriesCounter.keys()) {
        logVerbose(QString("%1 - %2").arg(key).arg(mNodeQuerriesCounter.value(key)));
    }

    {
        if (filter && filter->dataProcess()) {
            logVerbose("start processing file");
            QTemporaryFile decryptedFile(localFileInfo.absolutePath() + "/._sdrvtmpXXXXXX");
            if (!decryptedFile.open() || !tmpFile->open()) {
                mLastError = SxError(SxErrorCode::IOError, "failed to create temporary file", QCoreApplication::translate("SxErrorMessage", "failed to create temporary file"));
                return false;
            }
            XFile::makeInvisible(decryptedFile.fileName(), true);
            if (!_filter_data_process(tmpFile.get(), &decryptedFile, filter.get(), localFilePath, true)) {
                QString message = QT_TRANSLATE_NOOP("SxErrorMessage", "Filter %1 dataProcess failed");
                mLastError = SxError(SxErrorCode::FilterError, message.arg(filter->shortname()), QCoreApplication::translate("SxErrorMessage", message.toUtf8().constData()).arg(filter->shortname()));
                return false;
            }
            tmpFile->close();
            decryptedFile.close();
            decryptedFile.setAutoRemove(false);
            tmpName = decryptedFile.fileName();
        }
        else {
            tmpFile->setAutoRemove(false);
            tmpName = tmpFile->fileName();
        }
        tmpFile.reset(nullptr);

        if (mtime.isValid()) {
            localFileInfo.refresh();
            if (mtime != localFileInfo.lastModified()) {
                logWarning(QString("file %1 modified during download").arg(localFileInfo.absoluteFilePath()));
                mLastError = SxError(SxErrorCode::UnknownError, "file changed during download", QCoreApplication::translate("SxErrorMessage", "file changed during download"));
                return false;
            }
        }
    }
    renameFile:
    {
        fileEntry.mCreatedAt = 0;
        if (!XFile::safeRename(tmpName, localFilePath)) {
            logWarning("safe rename failed, create conflict file");
            QTemporaryFile conflicted(localFileInfo.absolutePath() +
                                      "/" +
                                      localFileInfo.baseName() +
                                      ".conflict-" +
                                      QDate::currentDate().toString("yyyyMMdd") +
                                      "-XXXXXX." +
                                      localFileInfo.completeSuffix());
            if(!conflicted.open()) {
                QString message = QT_TRANSLATE_NOOP("SxErrorMessage", "Failed to create conflict file: %1");
                mLastError = SxError(SxErrorCode::IOError,
                                     message.arg(conflicted.fileName()),
                                     QCoreApplication::translate("SxErrorMessage", message.toUtf8().constData()).arg(conflicted.errorString()));
                return false;
            }
            QString conflictedFilename = conflicted.fileName();
            conflicted.setAutoRemove(false);
            conflicted.close();
            if(!XFile::safeRename(tmpName, conflictedFilename)) {
                conflicted.remove();
                if(!QFile::rename(tmpName, conflictedFilename)) {
                    QString message = QT_TRANSLATE_NOOP("SxErrorMessage", "Failed to create conflict file: %1");
                    mLastError = SxError(SxErrorCode::IOError,
                                         message.arg(conflicted.fileName()),
                                         QCoreApplication::translate("SxErrorMessage", message.toUtf8().constData()).arg(conflicted.errorString()));
                    return false;
                }
                else
                    fileEntry.mCreatedAt = QFileInfo(conflictedFilename).lastModified().toTime_t();
            }
            else
                fileEntry.mCreatedAt = QFileInfo(conflictedFilename).lastModified().toTime_t();
            XFile::makeInvisible(conflictedFilename, false);
        }
        else {
            XFile::makeInvisible(localFilePath, false);
            fileEntry.mCreatedAt = QFileInfo(localFilePath).lastModified().toTime_t();
        }
    }

    fileEntry.mPath = path;
    fileEntry.mSize = file.mRemoteSize;
    fileEntry.mRevision = file.mRevision;
    fileEntry.mBlockSize = file.mBlockSize;
    fileEntry.mBlocks.clear();
    foreach (auto block, file.mBlocks) {
        fileEntry.mBlocks.append(block->mHash);
    }
    return true;

    io_error:
    logWarning("I/O error: "+mLastError.errorMessage());

    cleanMemory:
    foreach (auto pair, activeQueriesHelper) {
        delete pair.first;
        delete pair.second;
    }
    foreach (auto targets, activeQueries) {
        delete targets;
    }
    abortAllQueries();
    return false;
}

bool SxCluster::deleteFile(SxVolume *volume, QString path)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("volume: %1, file: %2").arg(volume->name()).arg(path));
    SxFile file(volume, path, "", true);
    SxJob job;
    if (!_deleteFile(file, job))
        return false;
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job))
            return false;
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    return true;
}

bool SxCluster::deleteFiles(SxVolume *volume, QStringList &filesToRemove, std::function<void(const QString&)> onRemove)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("volume: %1, files: %2").arg(volume->name()).arg(filesToRemove.join(", ")));
    QList <SxJob*> pending;
    QHash <SxJob*, QString> map;
    const int maxPending = 30;
    while (!filesToRemove.isEmpty() || !pending.isEmpty()) {
        foreach (SxJob* job, pending) {
            if (job->mStatus == SxJob::ERROR) {
                goto onError;
            }
            else if (job->mStatus == SxJob::OK) {
                logVerbose("REMOVED: " + map.value(job));
                onRemove(map.value(job));
                map.remove(job);
                pending.removeOne(job);
                delete job;
            }
            else {
                if (!_poll(*job))
                    goto onError;
            }
        }
        while (pending.size() < maxPending && !filesToRemove.isEmpty()) {
            QString path = filesToRemove.takeFirst();
            SxFile file(volume, path, "", true);
            SxJob *job = new SxJob();
            pending.append(job);
            map.insert(job, path);
            if (!_deleteFile(file, *job))
                goto onError;
            if (!_poll(*job))
                goto onError;
        }
        if (!pending.isEmpty())
            pending.last()->waitInterval();
    }
    return true;
    onError:
    logWarning("remove files failed");
    abortAllQueries();
    foreach (SxJob *job, pending) {
        delete job;
    }

    return false;
}

bool SxCluster::rename(SxVolume *volume, const QString &source, const QString &destination)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    logInfo(QString("volume: %1, source: %2, destination: %3").arg(volume->name()).arg(source).arg(destination));

    std::unique_ptr<SxFilter> filter(SxFilter::getActiveFilter(volume));
    if (filter && filter->dataPrepare()) {
        if (source.endsWith('/')) {
            if (!destination.endsWith('/'))
                return false;
            QList<SxFileEntry *> files;
            QString etag;
            if (!_listFiles(volume, source, true, files, etag))
                return false;
            qSort(files.begin(), files.end(), [](SxFileEntry *e1, SxFileEntry *e2)->bool {
                return e1->mSize > e2->mSize;
            });
            bool result = true;
            foreach (auto entry, files) {
                QString src = entry->path();
                QString dst = destination+src.mid(source.length());
                if (!_rename(volume, src, dst)) {
                    result = false;
                    break;
                }
            }
            foreach (auto entry, files) {
                delete entry;
            }
            return result;
        }
        else {
            return _rename(volume, source, destination);
        }
    }
    else {
        SxJob job;
        if (!_massRename(volume, source, destination, job))
            return false;
        while (job.mStatus == SxJob::PENDING) {
            job.waitInterval();
            if (!_poll(job))
                return false;
        }
        if (job.mStatus == SxJob::ERROR)
            return false;
        return true;
    }
}


bool SxCluster::checkFileConsistency(SxVolume *volume, const QString &path, QStringList& inconsistentRevisions)
{
    if (volume->nodeList().isEmpty())
        return false;
    QList<QPair<QString, QString>> list;
    foreach (QString node, volume->nodeList()) {
        QString revision;
        if (!getFileRevision(volume, path, node, revision))
            return false;
        list.append({node, revision});
    }
    bool consistent = true;
    QSet<QString> revisionSet;
    QString rev = list.first().second;
    foreach (auto pair, list) {
        if (pair.second != rev) {
            consistent = false;
        }
        if (!pair.second.isEmpty())
            revisionSet.insert(pair.second);
    }
    inconsistentRevisions.clear();
    if (!consistent) {
        logWarning(QString("inconsistency detected: %1%2").arg(volume->name()).arg(path));
        inconsistentRevisions = revisionSet.toList();
    }
    return true;
}

const QList<const SxVolume *> SxCluster::volumeList() const
{
    QList<const SxVolume*> list;
    foreach (SxVolume *volume, mVolumeList) {
        list.append(volume);
    }
    return list;
}

SxVolume *SxCluster::getSxVolume(const QString &volume)
{
    logEntry("");
    SxVolume *result = nullptr;
    foreach (SxVolume *vol, mVolumeList) {
        if (vol->name() == volume) {
            result = vol;
            break;
        }
    }
    return result;
}

bool SxCluster::changePassword(const QString &newToken)
{
    FunctionBlocker fb(this, Q_FUNC_INFO);
    if (fb.exit())
        return false;
    QByteArray usrkey = QByteArray::fromBase64(newToken.toUtf8());
    QByteArray keyHex = usrkey.mid(20, 20).toHex();
    QString queryString = "/.users/"+mUserInfo.username();
    QByteArray body = QString("{ \"userKey\": \""+keyHex+"\" }").toUtf8();

    SxQuery query(queryString, SxQuery::JOB_PUT, body);
    std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, mNodeList));
    if (!queryResult)
        return false;
    QJsonDocument json;
    if (!parseJson(queryResult.get(), json))
        return false;
    if (json.object().value("requestId").toString("").isEmpty() ||
            json.object().value("minPollInterval").toInt(0) <= 0 ||
            json.object().value("maxPollInterval").toInt(0) <= 0)
    {
        mLastError = SxError::errorBadReplyContent();
        logWarning(mLastError.errorMessage());
        return false;
    }
    SxJob job;
    bool oldAccount = true;
    if (!parseJobJson(json, queryResult->host(), job))
        return false;
    while (job.mStatus == SxJob::PENDING) {
        job.waitInterval();
        if (!_poll(job)) {
            if (mLastError.errorCode() == SxErrorCode::InvalidCredentials && oldAccount) {
                mSxAuth.setToken(newToken);
                mLastError = SxError();
                oldAccount = false;
                setAborted(false);
                continue;
            }
            return false;
        }
    }
    if (job.mStatus == SxJob::ERROR)
        return false;
    mSxAuth.setToken(newToken);
    return true;
}

bool SxCluster::changePassword(const QString &oldToken, const QString &newToken)
{
    if (!mSxAuth.checkToken(oldToken)) {
        mLastError = SxError(SxErrorCode::InvalidCredentials, "Invalid credentials", QCoreApplication::translate("SxErrorMessage", "Invalid credentials"));
        return false;
    }
    return changePassword(newToken);
}

bool SxCluster::getAllVolnodesEtag(SxVolume *volume, QList<QPair<QString, QString> > &result)
{
    result.clear();
    if (volume==nullptr && volume->nodeList().isEmpty())
        return false;
    QString queryString = "/"+volume->name()+"?o=list&recursive";
    foreach (auto node, volume->nodeList()) {
        SxQuery query(queryString, SxQuery::HEAD, QByteArray());
        QString etag;
        std::unique_ptr<SxQueryResult> queryResult(sendQuery(&query, {node}, etag));
        if (!queryResult)
            continue;
        etag = queryResult->etag();
        if (etag.isEmpty())
            continue;
        result.append({node, etag});
    }
    return !result.isEmpty();
}

const QStringList &SxCluster::nodes() const
{
    return mNodeList;
}

const SxUserInfo &SxCluster::userInfo() const
{
    return mUserInfo;
}

const QByteArray &SxCluster::uuid() const
{
    return mClusterUuid;
}

const SxAuth &SxCluster::auth() const
{
    return mSxAuth;
}

QString SxCluster::sxwebAddress() const
{
    if (mMeta.contains("sxweb_address")) {
        return QString::fromUtf8(mMeta.value("sxweb_address"));
    }
    return "";
}

QString SxCluster::sxshareAddress() const
{
    if (mMeta.contains("sxshare_address")) {
        return QString::fromUtf8(mMeta.value("sxshare_address"));
    }
    return "";
}

void SxCluster::abort()
{
    logEntry("");
    if (thread()==QThread::currentThread())
        abortAllQueries();
    else {
        setAborted(true);
        QMetaObject::invokeMethod(this, "abort");
    }
}

SxCluster::FunctionBlocker::FunctionBlocker(SxCluster *cluster, const QString &functionName)
{
    mCluster = cluster;
    if (mCluster->mCurrentFunctionName.isEmpty()) {
        mCluster->mCurrentFunctionName = functionName;
        mExit = false;
    }
    else {
        logDebug("Unable to call '"+ functionName + "', function '"+ mCluster->mCurrentFunctionName + "' already running");
        mExit = true;
    }
}

SxCluster::FunctionBlocker::~FunctionBlocker()
{
    if (!mExit) {
        mCluster->mCurrentFunctionName.clear();
    }
}

bool SxCluster::FunctionBlocker::exit() const
{
    return mExit;
}
