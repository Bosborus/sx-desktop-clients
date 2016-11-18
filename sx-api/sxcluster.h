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

#ifndef SXCLUSTER_H
#define SXCLUSTER_H

#include <functional>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>
#include <QSslCertificate>
#include <QEventLoop>
#include <functional>
#include <QMutex>

#include "sxurl.h"
#include "sxauth.h"
#include "sxuserinfo.h"
#include "sxvolume.h"
#include "sxmeta.h"
#include "sxfile.h"
#include "sxfileentry.h"
#include "sxjob.h"
#include "sxfilter/sx_input_args.h"
#include "sxerror.h"

class SxQuery;
class SxQueryResult;

class SXQuery {
    // TODO: REMOVE ME
};

class SxCluster : public QObject {
    static const int sTimeoutInitial = 30;
    static const int sTimeoutProgress = 10;
    static QString sClientVersion;

private:
    struct UploadJobInfo {
        SxVolume *volume;
        QString path;
        quint32 mTime;
        QDateTime lastPollTime;
        qint64 remoteSize;
        QStringList blocks;
    };
    class FunctionBlocker {
    public:
        FunctionBlocker(SxCluster *cluster, const QString &functionName);
        ~FunctionBlocker();
        bool exit() const;
    private:
        bool mExit;
        SxCluster *mCluster;
    };

    Q_OBJECT
    SxCluster(const SxAuth& auth, QByteArray uuid, const QStringList &nodes);
public:
    ~SxCluster();
    static SxCluster* initializeCluster(const SxAuth& auth, QByteArray uuid, std::function<bool(QSslCertificate &, bool)> checkSslCallback, QString &errorMessage);
    static bool getClusterUUID(const QString& cluster, const QString& initialAddress, const bool& ssl, const int& port, QString &uuid, QString &errorMessage, int timeout=-1);
    static bool getEnterpriseAuth(const QString &server, const QString &user, const QString &password, const QString &device, std::function<bool(const QSslCertificate &,bool)> checkCert, SxUrl &sx_url, QString &errorMessage);
    static void setClientVersion(const QString& version);
    static QString getClientVersion();
    void setCheckSslCallback(std::function<bool(QSslCertificate &, bool)> checkSslCallback);
    void setFilterInputCallback(std::function<int(sx_input_args&)> get_input);
    void setGetLocalBlocksCallback(std::function<bool(QFile *, qint64, int, const QStringList&, QSet<QString>&)> callback);
    void setFindIdenticalFilesCallback(std::function<bool(const QString&, qint64, int, const QStringList&, QList<QPair<QString, quint32>>&)> callback);
    SxError lastError() const;
    int getInput(sx_input_args &args) const;
    bool checkNetworkConfigurationChanged();
    bool rename(SxVolume* volume, const QString &source, const QString &destination);
    bool checkFileConsistency(SxVolume* volume, const QString &path, QStringList& inconsistentRevisions);
    bool reloadClusterNodes();
    bool reloadVolumes();
    bool reloadClusterMeta();
    bool listFileRevisions(SxVolume* volume, const QString &path, QList<std::tuple<QString, qint64, quint32> > &list);
    bool getFileRevision(SxVolume* volume, const QString &path, const QString &node, QString &revision);
    bool restoreFileRevision(SxVolume *volume, const QString &source, const QString &rev, const QString &destination);
    bool copyFile(SxVolume *srcVolume, const QString &source, SxVolume *dstVolume, const QString &destination);
    bool copyFiles(SxVolume *srcVolume, const QString &srcDir, const QStringList &files, SxVolume *dstVolume, const QString &dstDir, std::function<void(int, int)> progressCallback);
    bool moveFiles(SxVolume *srcVolume, const QString &srcDir, const QStringList &files, SxVolume *dstVolume, const QString &dstDir, std::function<void(int, int)> progressCallback);
    bool uploadFiles(const QString &localDir, const QStringList &files, SxVolume *dstVolume, const QString &dstDir, std::function<void(QString, qint64, qint64)> progressCallback, bool multipart);
    int  uploadJobsCount() const;
    bool pollUploadJobs(int jobLimit, std::function<void(QString, QString, SxError, QString, quint32)> callback);
    void clearUploadJobs();
    bool uploadFile(SxVolume* volume, QString path, QString localFile, SxFileEntry &fileRevision, std::function<void(QString, QString, SxError, QString, quint32)> callback=nullptr, bool multipart=false);
    bool createEmptyFile(SxVolume* volume, QString path);
    bool downloadFile(SxVolume* volume, QString path, QString localFile, SxFileEntry &fileEntry, int connectionLimit);
    bool downloadFile(SxVolume* volume, QString path, QString rev, QString localFile, SxFileEntry &fileEntry, int connectionLimit);
    bool deleteFile(SxVolume* volume, QString path);
    bool deleteFiles(SxVolume* volume, QStringList& filesToRemove, std::function<void(const QString &)>);
    const QList<const SxVolume*> volumeList() const;
    SxVolume *getSxVolume(const QString &volume);
    bool changePassword(const QString &newToken);
    bool changePassword(const QString& oldToken, const QString &newToken);
    bool getAllVolnodesEtag(SxVolume* volume, QList<QPair<QString, QString>> &result);

    // REST-API
    bool _listNodes(QStringList& nodeList);
    bool _getUserDetails(SxUserInfo &userInfo);
    bool _listVolumes(QList<SxVolume*> &volumeList);
    bool _locateVolume(SxVolume* volume, qint64 fileSize, int* blockSize);
    bool _getClusterMetadata(SxMeta &clusterMeta);
    bool _listFiles(SxVolume* volume, QList<SxFileEntry*>& fileList, QString &etag, const QString &after=QString(), const qint64 limit=0);
    bool _listFiles(SxVolume* volume, const QString path, bool recursive, QList<SxFileEntry*>& fileList, QString &etag, const QString &after=QString(), const qint64 limit=0);
    bool _getFile(SxFile &file, bool silence=false);
    bool _getFileMetadata(SxFile &file);
    bool _setFileMetadata(SxFile &file, const QJsonObject &jMeta);
    bool _initializeFile(SxFile &file);
    bool _initializeFileAddChunk(SxFile &file, int extendSeq);
    bool _createBlocks(const QString& uploadToken, const int blockSize, const QByteArray &data, const QStringList &nodes);
    bool _flushFile(SxFile &file, SxJob& job);
    bool _poll(SxJob& job);
    bool _deleteFile(SxFile &file, SxJob& job);
    bool _rename(SxVolume* volume, const QString &source, const QString &destination);
    bool _massRename(SxVolume* volume, const QString &source, const QString &destination, SxJob &job);
    bool _getBlocks(const QList<SxBlock*> &blockList, const int blockSize);
    bool _setVolumeCustomMeta(SxVolume *volume);
    SxQuery* _getBlocksMakeQuery(const QList<SxBlock*> &blockList, const int blockSize, QStringList &keys, QHash<QString, SxBlock *> &hash);
    bool _getBlocksProcessReply(SxQueryResult *query, const int blockSize, QStringList &keys, QHash<QString, SxBlock *> &hash);

    // GETTERS
    const QStringList& nodes() const;
    const SxUserInfo& userInfo() const;
    const QByteArray& uuid() const;
    const SxAuth &auth() const;
    QString sxwebAddress() const;
    QString sxshareAddress() const;

public slots:
    void abort();

signals:
    void sig_exit_loop();
    void sig_setProgress(qint64 size, qint64 speed);
    void sig_setDownloadSize(qint64 size);

private:
    bool checkSsl(QNetworkReply *reply, QTimer *timer, const QList<QSslError> &errors);
    SxQueryResult* sendQuery(SxQuery* query, QStringList targetList, const QString &etag=QString());
    QPair<SxQuery*, SxQueryResult*> querySelect(QHash<SxQuery *, QStringList *> &queries, const QString &etag=QString());
    inline bool testVolume(SxVolume* volume);
    inline bool testFile(SxFile &file);
    inline bool parseJson(SxQueryResult* queryResult, QJsonDocument& jDoc, bool silence=false);
    inline bool parseJobJson(QJsonDocument& json, QString pollTarget, SxJob &job);
    void abortAllQueries();
    QNetworkReply *sendNetworkRequest(SxQuery *query, QNetworkRequest req, bool seccondAttempt, int delay=0);
    void tryRemoveNetworkAccessManager(QNetworkAccessManager* manager);
    bool _filter_data_process(QFile *inFile, QFile *outFile, SxFilter* filter, QString file, bool download);
    void storeReplyTime(QNetworkReply* reply);
    bool aborted() const;
    void setAborted(bool aborted);

private:
    static const int sUploadJobsLimit = 10;
    QHash <SxJob*, UploadJobInfo> mUploadJobs;
    SxAuth mSxAuth;
    SxUserInfo mUserInfo;
    QStringList mNodeList;
    QNetworkAccessManager *mNetworkAccessManager;
    qint64 mTimeDrift;
    bool mViaSxCache;
    std::function<bool(QSslCertificate &, bool)> mCallbackConfirmCert;
    std::function<int(sx_input_args &)> mCallbackGetInput;
    std::function<bool(QFile *, qint64, int, const QStringList&, QSet<QString>&)> mGetLocalBlocks;
    std::function<bool(const QString&, qint64, int, const QStringList&, QList<QPair<QString, quint32>>&)> mFindIdenticalFilesCallback;
    QByteArray m_certFprint;
    QByteArray m_applianceCertFprint;
    QByteArray mClusterUuid;
    SxError mLastError;
    SxMeta mMeta;
    QHash<QNetworkReply*, SxQuery*> mActiveQueries;
    QHash<QNetworkReply*, QTimer*> mActiveTimers;
    QSet<QNetworkAccessManager*> mNetworkManagersToRemove;
    QList<SxVolume*> mVolumeList;
    bool m_Aborted;
    mutable QMutex mAbortedMutex;
    mutable QMutex mUploadJobMutex;
    QList<QHostAddress> mNetworkConfiguration;
    bool mUseApplianceNodeList;
    int mTimeoutProgress;
    int mTimeoutInitial;
    int mTimeoutMultiplier;
    QList<int> mInitialReplyTimes;
    QList<int> mProgressReplyTimes;
    int mInitialReplyTimeMax;
    int mInitialReplyTimeCount;
    int mProgressReplyTimeMax;
    int mProgressReplyTimeCount;
    QString mCurrentFunctionName;

    friend class SxCluster::FunctionBlocker;
};

#endif
