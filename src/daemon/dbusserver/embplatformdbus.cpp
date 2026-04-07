// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "embplatformdbus.h"

#include <QDBusMessage>
#include <QDBusConnection>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE
AIDAEMON_VECTORDB_USE_NAMESPACE

EmbPlatformDBus::EmbPlatformDBus(QObject *parent)
    : QObject(parent)
    , QDBusContext()
{
    // Register VectorDbReplyPtr as metatype for QMetaObject::invokeMethod
    qRegisterMetaType<VectorDbReplyPtr>("VectorDbReplyPtr");
}

void EmbPlatformDBus::start(QThread *dbusThread)
{
    qCInfo(logDBusServer) << "Starting EmbPlatformDBus service";
    auto session = QDBusConnection::sessionBus();
    if (!session.registerService("org.deepin.ai.daemon.EmbeddingPlatform")) {
        qCCritical(logDBusServer) << "Failed to register EmbPlatformDBus service: org.deepin.ai.daemon.EmbeddingPlatform";
        return;
    }

    if (!session.registerObject("/org/deepin/ai/daemon/EmbeddingPlatform",
                             this, QDBusConnection::ExportAllContents)) {
        qCCritical(logDBusServer) << "Failed to register QueryLang object: /org/deepin/ai/daemon/EmbeddingPlatform";
        return;
    }

    if (dbusThread)
        moveToThread(dbusThread);

    qCInfo(logDBusServer) << "EmbPlatformDBus service started successfully";

    interface.reset(new VectorDbInterface);
    interface->moveToThread(&workThread);
    workThread.start();

    connect(interface.data(), &VectorDbInterface::indexBuildCompleted, this,
            [this](const QString &appId, const QString &taskId, const QString &message){
            emit this->taskFinished(appId, taskId, message);
    });
}

QString EmbPlatformDBus::embeddingModels()
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::embeddingModels called";
        
    auto msg = message();
    setDelayedReply(true);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                                "embeddingModels",
                                Qt::QueuedConnection,
                                Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    qCDebug(logDBusServer) << "EmbPlatformDBus::embeddingModels completed successfully";
    return "";
}

QString EmbPlatformDBus::embeddingTexts(const QString &appId, const QStringList &texts, const QString &extensionParams)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::embeddingTexts called for appId:" << appId << "text:" << texts.size();

    auto msg = message();
    setDelayedReply(true);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                              "embeddingTexts",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(QStringList, texts),
                              Q_ARG(QString, extensionParams),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));

    return QString(); // Return value will be sent via delayed reply
}

QString EmbPlatformDBus::uploadDocuments(const QString &appId, const QStringList &files, const QString &extensionParams)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::uploadDocuments called for appId:" << appId << "files:" << files.size();
    
    auto msg = message();
    setDelayedReply(true);
    
    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(), 
                              "uploadDocuments",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(QStringList, files),
                              Q_ARG(QString, extensionParams),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    return QString(); // Return value will be sent via delayed reply
}

QString EmbPlatformDBus::deleteDocuments(const QString &appId, const QStringList &documentIds)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::deleteDocuments called for appId:" << appId << "documents:" << documentIds.size();
    
    auto msg = message();
    setDelayedReply(true);
    
    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(), 
                              "deleteDocuments",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(QStringList, documentIds),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    return ""; // Return value will be sent via delayed reply
}

QString EmbPlatformDBus::search(const QString &appId, const QString &query, const QString &extensionParams)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::search called for appId:" << appId << "query:" << query;

    auto msg = message();
    setDelayedReply(true);
    
    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(), 
                              "search",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(QString, query),
                              Q_ARG(QString, extensionParams),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    return QString(); // Return value will be sent via delayed reply
}

bool EmbPlatformDBus::cancelTask(const QString &taskId)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::cancelTask called for taskId:" << taskId;
    
    auto msg = message();
    setDelayedReply(true);
    
    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(), 
                              "cancelTask",
                              Qt::QueuedConnection,
                              Q_ARG(QString, taskId),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    return false; // Return value will be sent via delayed reply
}

QString EmbPlatformDBus::documentsInfo(const QString &appId, const QStringList &documentIds)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::documentsInfo called for appId:" << appId << "documentIds count:" << documentIds.size();
    
    auto msg = message();
    setDelayedReply(true);
    
    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(), 
                              "documentsInfo",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(QStringList, documentIds),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    return QString(); // Return value will be sent via delayed reply
}

QString EmbPlatformDBus::buildIndex(const QString &appId, const QString &docId, const QString &extensionParams)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::buildIndex called for appId:" << appId;
    
    auto msg = message();
    setDelayedReply(true);
    
    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(), 
                              "buildIndex",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(QString, docId),
                              Q_ARG(QString, extensionParams),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));
    
    return QString(); // Return value will be sent via delayed reply
}

QString EmbPlatformDBus::destroyIndex(const QString &appId, bool allIndex, const QString &extensionParams)
{
    qCInfo(logDBusServer) << "EmbPlatformDBus::destroyIndex called for appId:" << appId;

    auto msg = message();
    setDelayedReply(true);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                              "destroyIndex",
                              Qt::QueuedConnection,
                              Q_ARG(QString, appId),
                              Q_ARG(bool, allIndex),
                              Q_ARG(QString, extensionParams),
                              Q_ARG(VectorDbReplyPtr, VectorDbReplyPtr(new EmbPlatformDBusReply(msg))));

    return QString(); // Return value will be sent via delayed reply
}

EmbPlatformDBusReply::EmbPlatformDBusReply(QDBusMessage msg)
    : VectorDbReply()
    , dbusMsg(msg)
{

}

void EmbPlatformDBusReply::replyError(int code, const QString &message)
{
    QDBusMessage msg = dbusMsg.createError(static_cast<QDBusError::ErrorType>(code), message);
    QDBusConnection::sessionBus().send(msg);
}

void EmbPlatformDBusReply::replyString(const QString &message)
{
    QDBusMessage msg = dbusMsg.createReply(message);
    QDBusConnection::sessionBus().send(msg);
}

void EmbPlatformDBusReply::replyBool(bool ok)
{
    QDBusMessage msg = dbusMsg.createReply(ok);
    QDBusConnection::sessionBus().send(msg);
}
