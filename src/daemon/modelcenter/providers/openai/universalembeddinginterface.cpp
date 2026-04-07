// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "universalembeddinginterface.h"
#include "modelcenter/http/httpaccessmanager.h"
#include "modelcenter/http/httpeventloop.h"
#include "modelcenter/modelutils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QNetworkRequest>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

UniversalEmbeddingInterface::UniversalEmbeddingInterface(const QString &model, QObject *parent)
    : EmbeddingInterface(model, parent)
{

}

UniversalEmbeddingInterface::~UniversalEmbeddingInterface()
{
}

QJsonObject UniversalEmbeddingInterface::embeddings(const QStringList &texts, const QVariantHash &params)
{
    qCDebug(logModelCenter) << "Generating embeddings for" << texts.size() << "texts with model:" << model();

    // Prepare request data
    QJsonObject dataObject;
    dataObject.insert("model", model());
    dataObject.insert("encoding_format", "float");
    dataObject.insert("input", QJsonArray::fromStringList(texts));

    QJsonDocument jsonDocument(dataObject);
    const QByteArray &sendData = jsonDocument.toJson(QJsonDocument::Compact);

    // Create HTTP request
    QSharedPointer<HttpAccessmanager> httpAccessManager(new HttpAccessmanager(apiKey()));
    QNetworkRequest req = httpAccessManager->baseNetWorkRequest(apiUrl());

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::ContentLengthHeader, sendData.size());

    // Send request
    QNetworkReply *reply = httpAccessManager->post(req, sendData);

    // Wait for response
    HttpEventLoop loop(reply, "UniversalEmbeddingInterface::generateEmbeddings");
    loop.setHttpOutTime(timeOut());
    connect(this, &UniversalEmbeddingInterface::eventLoopAbort, &loop, &HttpEventLoop::abortReply);
    loop.exec();

    // Process response
    QNetworkReply::NetworkError netReplyError = reply->error();
    if (netReplyError != QNetworkReply::NoError) {
        qCWarning(logModelCenter) << "Network error during embedding generation:" << reply->errorString();
        setLastError(static_cast<int>(netReplyError));

        QJsonObject errorResponse;
        errorResponse["error"] = reply->errorString();
        errorResponse["code"] = static_cast<int>(netReplyError);
        return errorResponse;
    }

    QByteArray response = loop.getHttpResult();
    return processEmbeddingResponse(response);
}

void UniversalEmbeddingInterface::terminate()
{
    qCDebug(logModelCenter) << "Terminating embedding operations";
    emit eventLoopAbort();
}

void UniversalEmbeddingInterface::setApiUrl(const QString &apiUrl)
{
    m_apiUrl = apiUrl;
}

void UniversalEmbeddingInterface::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey;
}

QString UniversalEmbeddingInterface::apiUrl() const
{
    return m_apiUrl;
}

QString UniversalEmbeddingInterface::apiKey() const
{
    return m_apiKey;
}

int UniversalEmbeddingInterface::lastError() const
{
    return m_lastError;
}

void UniversalEmbeddingInterface::setLastError(int error)
{
    m_lastError = error;
}

int UniversalEmbeddingInterface::timeOut() const
{
    return m_timeOut;
}

void UniversalEmbeddingInterface::setTimeOut(int msec)
{
    m_timeOut = msec;
}

QJsonObject UniversalEmbeddingInterface::processEmbeddingResponse(const QByteArray &response)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (!jsonDoc.isObject()) {
        qCWarning(logModelCenter) << "Invalid JSON response from embedding API";
        QJsonObject errorResponse;
        errorResponse["error"] = "Invalid JSON response";
        errorResponse["code"] = 500;
        return errorResponse;
    }

    QJsonObject responseObj = jsonDoc.object();

    // Check for API errors
    if (responseObj.contains("error")) {
        qCWarning(logModelCenter) << "API error:" << responseObj["error"].toObject()["message"].toString();
        return responseObj;
    }

    return responseObj;
}
