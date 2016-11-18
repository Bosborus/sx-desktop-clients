#ifndef SHARECONFIG_H
#define SHARECONFIG_H

#include "abstractshareconfig.h"

class ScoutShareConfig : public AbstractShareConfig
{
public:
    ScoutShareConfig();
    void saveConfig() override;
    QString clusterToken() const override;
    QString volumePath(const QString &volume) const override;
    QStringList volumes() const override;
    qint64 expirationTime() const override;
    void setExpirationTime(qint64 expTime) override;
    QString notifyEmail() const override;
    void setNotifyEmail(const QString &email) override;
    QByteArray sxwebCertFp() const override;
    void setSxwebCertFp(const QByteArray &certFp) override;
};

#endif // SHARECONFIG_H
