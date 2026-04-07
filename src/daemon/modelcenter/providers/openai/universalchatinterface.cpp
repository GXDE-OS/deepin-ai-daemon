// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "universalchatinterface.h"
#include "aiconversation.h"
#include "modelcenter/http/httpaccessmanager.h"
#include "modelcenter/http/httpeventloop.h"
#include "modelcenter/modelutils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QHttpMultiPart>
#include <QString>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)


AIDAEMON_USE_NAMESPACE

InterfaceType UniversalChatInterface::type()
{
    return Chat;
}

UniversalChatInterface::UniversalChatInterface(const QString &model, QObject *parent)
    : ChatInterface(model, parent)
{

}

UniversalChatInterface::~UniversalChatInterface()
{

}

QJsonObject UniversalChatInterface::generate(const QString &content, bool stream, const QList<ChatHistory> &history,
                                             const QVariantHash &params)
{
    qCDebug(logModelCenter) << "Starting chat generation with model:" << model();
    AIConversation conversion;
    conversion.fromHistory(history);
    conversion.addUserData(content);

    QJsonObject dataObject;
    dataObject.insert("model", model());
    dataObject.insert("messages", conversion.getConversions());
    //dataObject.insert("temperature", qBound(0.0, 1.0, 2.0));
    dataObject.insert("stream", stream);

    ModelUtils::addFilteredParams(dataObject, params);

    QJsonDocument jsonDocument(dataObject);
    const QByteArray &sendData = jsonDocument.toJson(QJsonDocument::Compact);

    QHttpMultiPart *multipart = nullptr;

    QSharedPointer<HttpAccessmanager> httpAccessManager(new HttpAccessmanager(apiKey()));
    QNetworkRequest req = httpAccessManager->baseNetWorkRequest(apiUrl());

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "text/event-stream");

    QNetworkReply *reply;
    if (multipart) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "multipart/form-data; boundary=" + multipart->boundary());
        reply = httpAccessManager->post(req, multipart);
        multipart->setParent(reply);
    } else if (dataObject.isEmpty()) {
        reply = httpAccessManager->get(req);
    } else {
        req.setHeader(QNetworkRequest::ContentLengthHeader, sendData.size());
        reply = httpAccessManager->post(req, sendData);
    }

    HttpEventLoop loop(reply, "UniversalChatInterface::generate");
    loop.setHttpOutTime(timeOut());
    connect(&loop, &HttpEventLoop::sigReadyRead, this, &UniversalChatInterface::onReadDeltaContent);
    connect(this, &UniversalChatInterface::eventLoopAbort, &loop, &HttpEventLoop::abortReply);
    loop.exec();

    QNetworkReply::NetworkError netReplyError;
    bool isAuthError = httpAccessManager->isAuthenticationRequiredError();
    if (isAuthError) {
        qCWarning(logModelCenter) << "Authentication failed for API request";
        netReplyError = QNetworkReply::NetworkError::AuthenticationRequiredError;
    } else if (loop.getHttpStatusCode() == 429) {
        qCWarning(logModelCenter) << "Rate limit exceeded (HTTP 429)";
        netReplyError = QNetworkReply::NetworkError::InternalServerError;
    } else {
        netReplyError = loop.getNetWorkError();
        if (netReplyError != QNetworkReply::NoError) {
            qCWarning(logModelCenter) << "Network error occurred:" << netReplyError;
        }
    }

    setLastError(netReplyError);

    conversion.update(loop.getHttpResult());
    qCDebug(logModelCenter) << "Chat generation completed with status:" << netReplyError;

    QJsonObject response;
    response["content"] = conversion.getLastResponse();

    return response;
}

void UniversalChatInterface::terminate()
{
    qCDebug(logModelCenter) << "Terminating chat interface";
    emit eventLoopAbort();
}

void UniversalChatInterface::onReadDeltaContent(const QByteArray &content)
{
    if (content.isEmpty())
        return;
    const QJsonObject &deltacontent = AIConversation::parseContentString(content);
    if (deltacontent.contains("content"))
        emit sendDeltaContent(deltacontent.value("content").toString());
}

int UniversalChatInterface::lastError() const
{
    return m_lastError;
}

void UniversalChatInterface::setLastError(int error)
{
    m_lastError = error;
}

int UniversalChatInterface::timeOut() const
{
    return m_timeOut;
}

void UniversalChatInterface::setTimeOut(int msec)
{
    m_timeOut = msec;
}

QString UniversalChatInterface::apiUrl() const
{
    return m_apiUrl;
}

void UniversalChatInterface::setApiUrl(const QString &apiUrl)
{
    m_apiUrl = apiUrl;
}

QString UniversalChatInterface::apiKey() const
{
    return m_apiKey;
}

void UniversalChatInterface::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey;
}
