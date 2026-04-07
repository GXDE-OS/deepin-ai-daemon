// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EMBPLATFORMDBUS_H
#define EMBPLATFORMDBUS_H

#include "aidaemon_global.h"
#include "api/vectordbinterface.h"

#include <QObject>
#include <QDBusContext>
#include <QSharedPointer>
#include <QThread>

AIDAEMON_BEGIN_NAMESPACE

class EmbPlatformDBus : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.EmbeddingPlatform")
public:
    explicit EmbPlatformDBus(QObject *parent = nullptr);
    void start(QThread *dbusThread);
    QSharedPointer<AIDAEMON_VECTORDB_NAMESPACE::VectorDbInterface> handle() const {
        return interface;
    }
signals:
    void taskFinished(const QString &appid, const QString &taskId, const QString &message);
public slots:
    QString embeddingModels();
    QString embeddingTexts(const QString &appId, const QStringList &texts, const QString &extensionParams = QString());
    QString uploadDocuments(const QString &appId, const QStringList &files, const QString &extensionParams = QString());
    QString deleteDocuments(const QString &appId, const QStringList &documentIds);
    QString search(const QString &appId, const QString &query, const QString &extensionParams = QString());
    bool cancelTask(const QString &taskId);
    QString documentsInfo(const QString &appId, const QStringList &documentIds = {});
    QString buildIndex(const QString &appId, const QString &docId, const QString &extensionParams = QString());
    QString destroyIndex(const QString &appId, bool allIndex, const QString &extensionParams = QString());
private:
    QSharedPointer<AIDAEMON_VECTORDB_NAMESPACE::VectorDbInterface> interface;
    QThread workThread;
};

class EmbPlatformDBusReply : public AIDAEMON_VECTORDB_NAMESPACE::VectorDbReply
{
public:
    explicit EmbPlatformDBusReply(QDBusMessage msg);
    void replyError(int code, const QString &message) override;
    void replyString(const QString &message) override;
    void replyBool(bool ok) override;
protected:
    QDBusMessage dbusMsg;
};

AIDAEMON_END_NAMESPACE


#endif // EMBPLATFORMDBUS_H
