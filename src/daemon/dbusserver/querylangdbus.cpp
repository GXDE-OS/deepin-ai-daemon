// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "querylangdbus.h"

#include "modelcenter/modelhub/modelhubwrapper.h"
#include "modelcenter/http/httpeventloop.h"
#include "indexcenter/index/indexmanager.h"
#include "config/dconfigmanager.h"

#include <QDBusConnection>
#include <QLoggingCategory>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkAccessManager>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

QueryLangDBus::QueryLangDBus(QObject *parent)
    : QObject(parent)
    , QDBusContext()
{

}

void QueryLangDBus::start(QThread *dbusThread)
{
    qCInfo(logDBusServer) << "Starting QueryLangDBus service";
    auto session = QDBusConnection::sessionBus();
    if (!session.registerService("org.deepin.ai.daemon.QueryLang")) {
        qCCritical(logDBusServer) << "Failed to register QueryLang service: org.deepin.ai.daemon.QueryLang";
        return;
    }

    if (!session.registerObject("/org/deepin/ai/daemon/QueryLang",
                             this, QDBusConnection::ExportAllContents)) {
        qCCritical(logDBusServer) << "Failed to register QueryLang object: /org/deepin/ai/daemon/QueryLang";
        return;
    }

    if (dbusThread)
        moveToThread(dbusThread);

    qCInfo(logDBusServer) << "QueryLangDBus service started successfully";
}

QString QueryLangDBus::checkModel()
{
    const QString model_1_5 = "YouRong-1.5B";
    const QString model_7 = "YouRong-7B";

    QVariantHash infos;
    QString model;
    {
        for (const QVariantHash &mvh: ModelhubWrapper::modelsStatus()) {
            QString name = mvh.value("model").toString();
            if (name.compare(model_1_5, Qt::CaseInsensitive) == 0){
                infos = mvh;
                model = model_1_5;
                break;
            } else if (name.compare(model_7, Qt::CaseInsensitive) == 0){
                infos = mvh;
                model = model_7;
                break;
            }
        }
    }

    if (!model.isEmpty()) {
        qCDebug(logDBusServer) << "Found running model:" << model;
        return model;
    }

    QStringList modelList =  ModelhubWrapper::installedModels();
    if (modelList.contains(model_1_5, Qt::CaseInsensitive)) {
        qCDebug(logDBusServer) << "Found installed model:" << model_1_5;
        return model_1_5;
    }

    if (modelList.contains(model_7, Qt::CaseInsensitive)) {
        qCDebug(logDBusServer) << "Found installed model:" << model_7;
        return model_7;
    }

    qCWarning(logDBusServer) << "No suitable model found";
    return "";
}

QString QueryLangDBus::doQuery(const QByteArray &ba, const QUrl &url)
{
    qCDebug(logDBusServer) << "Sending query request to URL:" << url.toString();

    QNetworkAccessManager net;

    QNetworkRequest req;
    req.setUrl(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "text/event-stream");
    req.setHeader(QNetworkRequest::ContentLengthHeader, ba.size());

    auto reply = net.post(req, ba);

    HttpEventLoop loop(reply, "QueryLangDBus::doQuery");
    loop.setHttpOutTime(60 * 1000);
    loop.exec();

    QJsonDocument doc = QJsonDocument::fromJson(loop.getHttpResult());
    auto root = doc.object();
    for (const QJsonValue &jv : root.value("data").toArray()) {
        auto obj = jv.toObject();
        if (obj.value("object").toString() == QString("dsl"))
            return obj.value("dsl").toString();
    }

    qCWarning(logDBusServer) << "No DSL object found in response";
    return "";
}

QString QueryLangDBus::Query(const QString &content)
{
    qCInfo(logDBusServer) << "Processing query request for content:" << content;

    if (content.isEmpty()) {
        qCWarning(logDBusServer) << "Empty query content received";
        return "";
    }

    QString model = checkModel();
    if (model.isEmpty()) {
        qCWarning(logDBusServer) << "No suitable model available for query";
        return "";
    }

    qCDebug(logDBusServer) << "Using model for query:" << model;
    ModelhubWrapper wrapper(model);
    wrapper.setKeepLive(true);

    if (!wrapper.ensureRunning()) {
        qCWarning(logDBusServer) << "Failed to ensure model running:" << model;
        return "";
    }

    QString url = wrapper.urlPath("/dsl");
    qCDebug(logDBusServer) << "Query URL:" << url;

    QJsonObject dataObject;
    dataObject.insert("input", content);
    QJsonDocument jsonDocument(dataObject);
    const QByteArray &sendData = jsonDocument.toJson(QJsonDocument::Compact);

    return doQuery(sendData, url);
}

bool QueryLangDBus::Enable()
{
    QStringList modelList =  ModelhubWrapper::installedModels();
    return modelList.contains("YouRong-1.5B") || modelList.contains("YouRong-7B");
}

void QueryLangDBus::SetSemanticOn(bool isTrue)
{
    IndexManager::instance()->onSemanticAnalysisChecked(isTrue, true);
}

QString QueryLangDBus::GetSemanticStatus()
{
    if (!ConfigManagerIns->value(SEMANTIC_ANALYSIS_GROUP, ENABLE_SEMANTIC_ANALYSIS, false).toBool()) {
        return "{\"enable\":false}";
    }

    QJsonObject rootJson;
    rootJson.insert("enable", true);
    int completion = ConfigManagerIns->value(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED, false).toBool() ? 1 : 0;
    rootJson.insert("completion", completion);
    if (completion == 1) {
        rootJson.insert("updatetime", ConfigManagerIns->value(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED_TIME_S, QDateTime::currentDateTimeUtc().toSecsSinceEpoch()).toLongLong());
    }
    return QJsonDocument(rootJson).toJson(QJsonDocument::JsonFormat::Compact);
}
