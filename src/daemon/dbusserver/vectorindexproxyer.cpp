#include "vectorindexproxyer.h"
#include "data/datamanager.h"

#include "indexcenter/index/index_define.h"

#include <QLoggingCategory>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QElapsedTimer>
#include <QFileInfo>

#include <math.h>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE
AIDAEMON_VECTORDB_USE_NAMESPACE

VectorIndexProxyer::VectorIndexProxyer(QSharedPointer<vector_db::VectorDbInterface> ifs, QObject *parent)
    : QObject(parent)
    , interface(ifs)
{
    m_whiteList << kUosAIAssistant;
    m_whiteList << kSystemAssistantKey;
}

VectorIndexProxyer::~VectorIndexProxyer()
{

}

void VectorIndexProxyer::start(QThread *dbusThread)
{
    auto connVi { QDBusConnection::sessionBus() };
    if (!connVi.registerService("org.deepin.ai.daemon.VectorIndex")) {
        qCCritical(logDBusServer) << "Failed to register VectorIndex service: org.deepin.ai.daemon.VectorIndex";
        return;
    }

    qCInfo(logDBusServer) << "Initializing VectorIndex DBus service";
    if (!connVi.registerObject("/org/deepin/ai/daemon/VectorIndex", this
                               , QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        qCCritical(logDBusServer) << "Failed to register VectorIndex object: /org/deepin/ai/daemon/VectorIndex";
        return;
    }

    qCInfo(logDBusServer) << "VectorIndex DBus service initialized successfully";

    if (dbusThread)
        moveToThread(dbusThread);

    connect(interface.data(), &VectorDbInterface::indexBuildStarted, this, [this](const QString &appId, const QString &taskId) {
        Q_UNUSED(taskId)
        if (!m_whiteList.contains(appId))
            return;

         emit IndexStatus(appId, QStringList(), 0);
    });

    connect(interface.data(), &VectorDbInterface::indexBuildCompleted, this, [this](const QString &appId, const QString &taskId, const QString &message) {
        Q_UNUSED(taskId)
        Q_UNUSED(message)
        if (!m_whiteList.contains(appId))
            return;

         emit IndexStatus(appId, QStringList(), 0);
    });

    connect(interface.data(), &VectorDbInterface::documentsDeleted, this, [this](const QString &appId, const QStringList &docIDs, const QStringList &filePaths) {
        Q_UNUSED(docIDs)

        if (!m_whiteList.contains(appId))
            return;

         emit IndexDeleted(appId, filePaths);
    });
}

bool VectorIndexProxyer::Create(const QString &appID, const QStringList &files)
{
    if (!m_whiteList.contains(appID))
        return false;

    // check file
    {
        QMap<QString, QString> docs;
        {
            VectorIndexReply *reply = new VectorIndexReply;
            VectorDbReplyPtr replyPtr(reply);

            // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
            QMetaObject::invokeMethod(interface.data(),
                                        "documentsInfo",
                                        Qt::QueuedConnection,
                                        Q_ARG(QString, appID),
                                        Q_ARG(QStringList, QStringList()),
                                        Q_ARG(VectorDbReplyPtr, replyPtr));
            reply->waitForFinished(5000);
            QString value = reply->value<QString>();
            if (reply->isError()) {
                qCWarning(logDBusServer) << "fail to get files:" << value;
                // 需在本函数退出后发送信号
                QMetaObject::invokeMethod(this, "IndexStatus", Qt::QueuedConnection,
                                          Q_ARG(QString, appID),
                                          Q_ARG(QStringList, files),  //使用旧版的状态接口
                                          Q_ARG(int, -2));
                return false;
            }

            QJsonObject root = QJsonDocument::fromJson(value.toUtf8()).object();
            for (QJsonValue tmp : root.value("results").toArray()) {
                QJsonObject docObj = tmp.toObject();
                QString file = docObj["file_path"].toString();
                QFileInfo info(file);
                docs.insert(info.fileName(), docObj.value("id").toString());
            }
        }

        // file is existed
        QStringList ids;
        for (const QString &file : files) {
            QFileInfo info(file);
            auto it = docs.find(info.fileName());
            if (it != docs.end()) {
                qCWarning(logDBusServer) << file << "is existed." << it.key() << it.value();
                // 需在本函数退出后发送信号
                QMetaObject::invokeMethod(this, "IndexStatus", Qt::QueuedConnection,
                                          Q_ARG(QString, appID),
                                          Q_ARG(QStringList, QStringList()),  //使用新版的状态接口
                                          Q_ARG(int, 0));
                return false;
            }
        }
    }

    // upload file
    QStringList docIds;
    {
        VectorIndexReply *reply = new VectorIndexReply;
        VectorDbReplyPtr replyPtr(reply);

        QVariantHash hashParams;
        // fixed model
        hashParams.insert(ExtensionParams::kModelKey, dependModel());
        QString params = QJsonDocument(QJsonObject::fromVariantHash(hashParams)).toJson(QJsonDocument::Compact);

        QMetaObject::invokeMethod(interface.data(),
                                    "uploadDocuments",
                                    Qt::QueuedConnection,
                                    Q_ARG(QString, appID),
                                    Q_ARG(QStringList, files),
                                    Q_ARG(QString, params),
                                    Q_ARG(VectorDbReplyPtr, replyPtr));
        reply->waitForFinished(5000);
        QString value = reply->value<QString>();
        if (reply->isError()) {
            qCWarning(logDBusServer) << "fail to upload files:" << value;
            // 需在本函数退出后发送信号
            QMetaObject::invokeMethod(this, "IndexStatus", Qt::QueuedConnection,
                                      Q_ARG(QString, appID),
                                      Q_ARG(QStringList, QStringList()),  //使用新版的状态接口
                                      Q_ARG(int, 0));
            return false;
        }

        QJsonObject root = QJsonDocument::fromJson(value.toUtf8()).object();
        for (QJsonValue tmp : root.value("results").toArray()) {
            QJsonObject docObj = tmp.toObject();
            QString id = docObj.value("documentID").toString();
            if (!id.isEmpty())
                docIds.append(id);
        }
    }

    // build index
    if (!docIds.isEmpty()) {
        VectorIndexReply *reply = new VectorIndexReply;
        VectorDbReplyPtr replyPtr(reply);

        for (const QString &id : docIds) {
            QMetaObject::invokeMethod(interface.data(),
                                        "buildIndex",
                                        Qt::QueuedConnection,
                                        Q_ARG(QString, appID),
                                        Q_ARG(QString, id),
                                        Q_ARG(QString, QString()),
                                        Q_ARG(VectorDbReplyPtr, replyPtr));
            qCDebug(logDBusServer) << "build index doc:"<< id;
        }
    }

    return true;
}

bool VectorIndexProxyer::Delete(const QString &appID, const QStringList &files)
{
    if (!m_whiteList.contains(appID) || files.isEmpty())
        return false;

    QMap<QString, QString> docs;
    {
        VectorIndexReply *reply = new VectorIndexReply;
        VectorDbReplyPtr replyPtr(reply);

        // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
        QMetaObject::invokeMethod(interface.data(),
                                    "documentsInfo",
                                    Qt::QueuedConnection,
                                    Q_ARG(QString, appID),
                                    Q_ARG(QStringList, QStringList()),
                                    Q_ARG(VectorDbReplyPtr, replyPtr));
        reply->waitForFinished(5000);
        QString value = reply->value<QString>();
        if (reply->isError()) {
            qCWarning(logDBusServer) << "fail to get files:" << value;
            return false;
        }

        QJsonObject root = QJsonDocument::fromJson(value.toUtf8()).object();
        for (QJsonValue tmp : root.value("results").toArray()) {

            QJsonObject docObj = tmp.toObject();
            QString file = docObj["file_path"].toString();
            docs.insert(file, docObj.value("id").toString());
        }
    }

    QStringList ids;
    for (const QString &file : files) {
        auto id = docs.value(file);
        if (!id.isEmpty())
            ids.append(id);
    }

    if (ids.isEmpty())
        return false;

    VectorIndexReply *reply = new VectorIndexReply;
    VectorDbReplyPtr replyPtr(reply);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                                "deleteDocuments",
                                Qt::QueuedConnection,
                                Q_ARG(QString, appID),
                                Q_ARG(QStringList, ids),
                                Q_ARG(VectorDbReplyPtr, replyPtr));
    return true;
}

QString VectorIndexProxyer::DocFiles(const QString &appID)
{
    if (!m_whiteList.contains(appID))
        return "";

    VectorIndexReply *reply = new VectorIndexReply;
    VectorDbReplyPtr replyPtr(reply);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                                "documentsInfo",
                                Qt::QueuedConnection,
                                Q_ARG(QString, appID),
                                Q_ARG(QStringList, QStringList()),
                                Q_ARG(VectorDbReplyPtr, replyPtr));
    reply->waitForFinished(10000);
    QString value = reply->value<QString>();
    if (reply->isError()) {
        qCWarning(logDBusServer) << "DocFiles error:" << value;
        return QString(R"({"error":"%0"})").arg(value);
    }

    QJsonArray datas;
    QJsonObject root = QJsonDocument::fromJson(value.toUtf8()).object();

    for (QJsonValue tmp : root.value("results").toArray()) {

        QJsonObject docObj = tmp.toObject();
        QString file = docObj["file_path"].toString();

        // convert status
        int status = docObj.value("processing_status").toInt();
        if (status == DataManager::ProcessingStatus::Vectorized) {
            status = EmbeddingStatus::Success;
        } else {
            status = EmbeddingStatus::EmbeddingStart;
        }

        QJsonObject res;
        res.insert("model", docObj.value("model"));
        res.insert("doc", docObj["file_path"]);
        res.insert("status", status);
        datas.append(res);
    }

    if (!datas.isEmpty()) {
        QJsonObject respose;
        respose.insert("result", datas);
        respose.insert("version", 2.0);
        value = QJsonDocument(respose).toJson(QJsonDocument::Compact);
    }

    return value;
}

bool VectorIndexProxyer::Enable()
{
    VectorIndexReply *reply = new VectorIndexReply;
    VectorDbReplyPtr replyPtr(reply);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                                "embeddingModels",
                                Qt::QueuedConnection,
                                Q_ARG(VectorDbReplyPtr, replyPtr));

    reply->waitForFinished(5000);

    QSet<QString> models;
    if (reply->isError()) {
        qCWarning(logDBusServer) << "invalid reply form embeddingModels" << reply->value<QString>();
        return false;
    } else {
        QJsonDocument doc = QJsonDocument::fromJson(reply->value<QString>().toUtf8());
        auto root = doc.object().value("results");
        if (root.isArray()) {
            QJsonArray modelsArray = root.toArray();
            for (const QJsonValue &value : modelsArray) {
                if (value.isObject()) {
                    QJsonObject modelObj = value.toObject();
                    if (modelObj.contains("model")) {
                        QString modelId = modelObj["model"].toString();
                        if (!modelId.isEmpty())
                            models.insert(modelId);
                    }
                }
            }
        }
    }

    qCDebug(logDBusServer) << "get models form embeddingModels" << models;

    return !models.isEmpty();
}

QString VectorIndexProxyer::Search(const QString &appID, const QString &query, int topK)
{
    if (!m_whiteList.contains(appID))
        return "";

    VectorIndexReply *reply = new VectorIndexReply;
    VectorDbReplyPtr replyPtr(reply);

    QVariantHash hashParams;
    // fixed model
    hashParams.insert(ExtensionParams::kModelKey, dependModel());
    hashParams.insert(ExtensionParams::kTopkKey, topK);
    QString params = QJsonDocument(QJsonObject::fromVariantHash(hashParams)).toJson(QJsonDocument::Compact);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                                "search",
                                Qt::QueuedConnection,
                                Q_ARG(QString, appID),
                                Q_ARG(QString, query),
                                Q_ARG(QString, params),
                                Q_ARG(VectorDbReplyPtr, replyPtr));

    reply->waitForFinished(60000);
    QString value = reply->value<QString>();
    if (reply->isError()) {
        qCWarning(logDBusServer) << "search error:" << value;
        return QString(R"({"%0":"%1", "error":"%2"})").arg(ExtensionParams::kModelKey).arg(dependModel()).arg(value);
    }

    QJsonArray datas;
    QJsonObject root = QJsonDocument::fromJson(value.toUtf8()).object();
    for (const QJsonValue &tmp : root.value("results").toArray()) {
        QJsonObject data = tmp.toObject();
        QJsonObject chunk = data.value("chunk").toObject();

        QJsonObject res;
        res.insert("model", data.value("model"));
        float sim = static_cast<float>(data.value("similarity").toDouble());
        float dis = std::sqrt(2 * (1 - sim));
        res.insert("distance", dis);
        res.insert("similarity", data.value("similarity"));
        res.insert("content", chunk.value("content"));
        res.insert("source", chunk.value("file"));
        datas.append(res);
    }

    if (!datas.isEmpty()) {
        QJsonObject respose;
        respose.insert("result", datas);
        respose.insert("version", 1.0);
        value = QJsonDocument(respose).toJson(QJsonDocument::Compact);
    }

    return value;
}

QString VectorIndexProxyer::embeddingTexts(const QString &appID, const QStringList &texts)
{
    VectorIndexReply *reply = new VectorIndexReply;
    VectorDbReplyPtr replyPtr(reply);

    // fixed model
    QVariantHash hashParams;
    hashParams.insert(ExtensionParams::kModelKey, dependModel());
    const QString params = QJsonDocument(QJsonObject::fromVariantHash(hashParams)).toJson(QJsonDocument::Compact);

    // Use QMetaObject::invokeMethod to call VectorDbInterface in its thread
    QMetaObject::invokeMethod(interface.data(),
                                "embeddingTexts",
                                Qt::QueuedConnection,
                                Q_ARG(QString, appID),
                                Q_ARG(QStringList, texts),
                                Q_ARG(QString, params),
                                Q_ARG(VectorDbReplyPtr, replyPtr));

    reply->waitForFinished(texts.size() * 10000);

    QSet<QString> models;
    QString value = reply->value<QString>();
    if (reply->isError()) {
        qCWarning(logDBusServer) << "invalid reply form embeddingModels" << value;
        return QString(R"({"%0":"%1", "error":"%2"})").arg(ExtensionParams::kModelKey).arg(dependModel()).arg(value);
    }

    return value;
}

QString VectorIndexProxyer::getAutoIndexStatus(const QString &appID)
{
    qCInfo(logDBusServer) << "getAutoIndexStatus is DEPRECATED";
    return R"({"enable":false})";
}

void VectorIndexProxyer::saveAllIndex(const QString &appID)
{
    qCInfo(logDBusServer) << "saveAllIndex is DEPRECATED";
}

void VectorIndexProxyer::setAutoIndex(const QString &appID, bool on)
{
    qCInfo(logDBusServer) << "setAutoIndex is DEPRECATED";
}

VectorIndexReply::VectorIndexReply() : VectorDbReply()
{

}

VectorIndexReply::~VectorIndexReply()
{

}

void VectorIndexReply::replyError(int code, const QString &message)
{
    error = code;
    content = message;
    finished = true;
}

void VectorIndexReply::replyString(const QString &message)
{
    error = 0;
    content = message;
    finished = true;
}

void VectorIndexReply::replyBool(bool ok)
{
    error = 0;
    content = ok;
    finished = true;
}

void VectorIndexReply::waitForFinished(int timeout)
{
    QElapsedTimer time;
    time.start();
    while (!finished) {
        if (timeout > 0 && time.hasExpired(timeout))
            return;
        QThread::msleep(5);
    }
}
