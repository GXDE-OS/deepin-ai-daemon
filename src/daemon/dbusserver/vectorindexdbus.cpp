// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vectorindexdbus.h"
#include "config/dconfigmanager.h"
#include "indexcenter/index/index_define.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QThread>
#include <QDBusConnection>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

VectorIndexDBus::VectorIndexDBus(QObject *parent) : QObject(parent)
{
    //m_whiteList << kGrandVectorSearch;
    m_whiteList << kUosAIAssistant;
    m_whiteList << kSystemAssistantKey;
}

VectorIndexDBus::~VectorIndexDBus()
{
    for (auto it : embeddingWorkerwManager.values()) {
        delete it;
        it = nullptr;
    }
}

bool VectorIndexDBus::Create(const QString &appID, const QStringList &files)
{
    qCInfo(logDBusServer) << "Creating index for app:" << appID << "files count:" << files.size();
    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to create index: no worker available for app:" << appID;
        return false;
    }

    // run in thread
    QMetaObject::invokeMethod(embeddingWorker, "doCreateIndex", Q_ARG(QStringList, files));
    return true;
}

bool VectorIndexDBus::Delete(const QString &appID, const QStringList &files)
{
    qCInfo(logDBusServer) << "Deleting index for app:" << appID << "files count:" << files.size();
    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to delete index: no worker available for app:" << appID;
        return false;
    }

    // run in thread
    QMetaObject::invokeMethod(embeddingWorker, "doDeleteIndex", Q_ARG(QStringList, files));
    return false;
}

bool VectorIndexDBus::Enable()
{
    return (bgeModel->isRunning()) || (ModelhubWrapper::isModelhubInstalled() &&
                                       ModelhubWrapper::isModelInstalled(dependModel()));
}

QString VectorIndexDBus::DocFiles(const QString &appID)
{
    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to get doc files: no worker available for app:" << appID;
        return {};
    }

    return embeddingWorker->getDocFile();
}

QString VectorIndexDBus::Search(const QString &appID, const QString &query, int topK)
{
    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to process search: no worker available for app:" << appID;
        return "";
    }

    return embeddingWorker->doVectorSearch(query, topK);
}

QString VectorIndexDBus::getAutoIndexStatus(const QString &appID)
{
    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to get auto index status: no worker available for app:" << appID;
        return R"({"enable":false})";
    }

    bool on = ConfigManagerIns->value(AUTO_INDEX_GROUP, appID + "." + AUTO_INDEX_STATUS, false).toBool();
    if (!on) {
        qCDebug(logDBusServer) << "Auto index is disabled for app:" << appID;
        return R"({"enable":false})";
    }

    QVariantHash hash;
    hash.insert("enable", true);
    int st = embeddingWorker->createAllState() == GET_INDEX_STATUS_CODE(INDEX_STATUS_CREATING) ? 0 : 1;
    hash.insert("completion", st);

    if (st == 1) {
        qint64 time = embeddingWorker->getIndexUpdateTime();
        hash.insert("updatetime", time);
    }

    QString str = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantHash(hash)).toJson());
    return str;
}

void VectorIndexDBus::setAutoIndex(const QString &appID, bool on)
{
    return;

    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to set auto index: no worker available for app:" << appID;
        return;
    }

    ConfigManagerIns->setValue(AUTO_INDEX_GROUP, appID + "." + AUTO_INDEX_STATUS, on);
    if (on) {
        embeddingWorker->setWatch(false);
        // run in thread
        QMetaObject::invokeMethod(embeddingWorker, "onCreateAllIndex");
        embeddingWorker->setWatch(true);
    } else {
        embeddingWorker->stop();
        embeddingWorker->setWatch(false);
    }
}

void VectorIndexDBus::saveAllIndex(const QString &appID)
{
    EmbeddingWorker *embeddingWorker = ensureWorker(appID);
    if (!embeddingWorker) {
        qCWarning(logDBusServer) << "Failed to save index: no worker available for app:" << appID;
        return;
    }
    embeddingWorker->saveAllIndex();
}

EmbeddingWorker *VectorIndexDBus::ensureWorker(const QString &appID)
{
    EmbeddingWorker *worker = embeddingWorkerwManager.value(appID);
    if (m_whiteList.contains(appID) && !worker) {
        worker = new EmbeddingWorker(appID);
        initEmbeddingWorker(worker);
        embeddingWorkerwManager.insert(appID, worker);
    }

    return worker;
}

QJsonObject VectorIndexDBus::embeddingApi(const QStringList &texts, void *user)
{
    VectorIndexDBus *self = static_cast<VectorIndexDBus *>(user);
    qCDebug(logDBusServer) << "Processing embedding request for texts count:" << texts.size();

#ifdef AIPC_AI_DAEMON
    self->bgeModel->setRequestHost("127.0.0.1", 7778);
    QNetworkRequest request(self->bgeModel->urlPath("/v1/embeddings"));
#endif

#ifndef AIPC_AI_DAEMON
    //deepin-modelhub bge
    if (!self->bgeModel->ensureRunning()) {
        qCWarning(logDBusServer) << "Failed to ensure BGE model running";
        return {};
    }
    QNetworkRequest request(self->bgeModel->urlPath("/embeddings"));
#endif

    QNetworkAccessManager manager;
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonArray jsonArray;
    for (const QString &str : texts) {
        jsonArray.append(str);
    }
    QJsonValue jsonValue = QJsonValue(jsonArray);

    QJsonObject data;

#ifdef AIPC_AI_DAEMON
    data["model"] = "bge-large-zh-v1.5";
#endif

    data["input"] = jsonValue;

    QJsonDocument jsonDocHttp(data);
    QByteArray jsonDataHttp = jsonDocHttp.toJson();
    QJsonDocument replyJson;
    QNetworkReply *reply = manager.post(request, jsonDataHttp);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        replyJson = QJsonDocument::fromJson(response);
    } else {
        qCWarning(logDBusServer) << "Embedding API request failed:" << reply->errorString();
    }
    reply->deleteLater();
    QJsonObject obj = {};
    if (replyJson.isObject()) {
        obj = replyJson.object();
        return obj;
    }
    return {};
}

void VectorIndexDBus::initBgeModel()
{
    QTimer::singleShot(100, this, [](){
        if (ModelhubWrapper::isModelhubInstalled()) {
            if (!ModelhubWrapper::isModelInstalled(dependModel()))
                qCWarning(logDBusServer) << "VectorIndex needs model" << dependModel() << "but it is not available";
        } else {
            qCWarning(logDBusServer) << "VectorIndex depends on deepin modelhub, but it is not available";
        }
    });

    bgeModel = new ModelhubWrapper(dependModel(), this);
}

QString VectorIndexDBus::embeddingTexts(const QString &appID, const QStringList &texts)
{
    Q_UNUSED(appID)

    return QJsonDocument(embeddingApi(texts, this)).toJson(QJsonDocument::Compact);
}

void VectorIndexDBus::start(QThread *dbusThread)
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

    initBgeModel();
    for (const QString &app : m_whiteList) {
         bool on = ConfigManagerIns->value(AUTO_INDEX_GROUP, app + "." + AUTO_INDEX_STATUS, false).toBool();
         if (!on)
             continue;

         auto work = ensureWorker(app);
         work->setWatch(true);
    }
}

void VectorIndexDBus::initEmbeddingWorker(EmbeddingWorker *ew)
{
    if (!ew) {
        qCWarning(logDBusServer) << "Failed to initialize embedding worker: null pointer";
        return;
    }

    ew->setEmbeddingApi(embeddingApi, this);
    connect(ew, &EmbeddingWorker::statusChanged, this, &VectorIndexDBus::IndexStatus);
    connect(ew, &EmbeddingWorker::indexDeleted, this, &VectorIndexDBus::IndexDeleted);
}
