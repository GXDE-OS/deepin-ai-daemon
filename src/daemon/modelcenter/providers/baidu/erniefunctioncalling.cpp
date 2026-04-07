// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erniefunctioncalling.h"
#include "erniechat.h"
#include "ernieconversation.h"
#include "modelcenter/modelutils.h"

#include "modelcenter/http/httpaccessmanager.h"
#include "modelcenter/http/httpeventloop.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QHttpMultiPart>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)
AIDAEMON_USE_NAMESPACE

using namespace ernie;

ErnieFunctionCalling::ErnieFunctionCalling(QSharedPointer<ErnieChat> ifs, QObject *parent)
    : FunctionCallingInterface(qSharedPointerCast<ChatInterface>(ifs), parent)
{

}

QJsonObject ErnieFunctionCalling::parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params)
{
    auto chat = ernieChat();
    QString urlPath = chat->getApiPath().isEmpty() ? chat->modelUrl(model()) : chat->getApiPath();
    qCDebug(logModelCenter) << "Preparing function calling request to:" << urlPath;
    
    if (urlPath.isEmpty() || func.isEmpty()) {
        qCWarning(logModelCenter) << "Function calling aborted - empty URL path or function list";
        return {};
    }

    ErnieConversation conversation;
    conversation.addUserData(content);
    conversation.setFunctions(func);

    QJsonObject dataObject;
    dataObject.insert("messages", conversation.getConversions());
    dataObject.insert("functions", conversation.getFunctions());
    dataObject.insert("temperature", 0.1);

    ModelUtils::addFilteredParams(dataObject, params);

    QJsonDocument jsonDocument(dataObject);
    const QByteArray &sendData = jsonDocument.toJson(QJsonDocument::Compact);

    QString accessToken = chat->updateAccessToken(chat->getApiKey(), chat->getApiSecret());
    if (accessToken.isEmpty()) {
        qCCritical(logModelCenter) << "Failed to get access token for function calling";
        return {};
    }

    QUrl url(urlPath);
    QUrlQuery query;
    query.addQueryItem("access_token", accessToken);
    url.setQuery(query);

    QHttpMultiPart *multipart = nullptr;

    QSharedPointer<HttpAccessmanager> httpAccessManager(new HttpAccessmanager(chat->getApiKey()));
    QNetworkRequest req = httpAccessManager->baseNetWorkRequest(url);

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    //req.setRawHeader("Accept", "text/event-stream");

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

    HttpEventLoop loop(reply, "ErnieChat::functionCalling");
    loop.setHttpOutTime(60 * 1000);
    connect(chat.data(), &ErnieChat::eventLoopAbort, &loop, &HttpEventLoop::abortReply);
    loop.exec();

    QNetworkReply::NetworkError netReplyError;
    bool isAuthError = httpAccessManager->isAuthenticationRequiredError();
    if (isAuthError) {
        qCCritical(logModelCenter) << "Authentication failed for function calling request";
        netReplyError = QNetworkReply::NetworkError::AuthenticationRequiredError;
    } else if (loop.getHttpStatusCode() == 429) {
        qCWarning(logModelCenter) << "Rate limit exceeded for function calling";
        netReplyError = QNetworkReply::NetworkError::InternalServerError;
    } else {
        netReplyError = loop.getNetWorkError();
    }

    conversation.update(loop.getHttpResult());

    QJsonObject response;
    response["content"] = conversation.getLastResponse();

    QJsonArray tools = conversation.getLastTools();
    if (!tools.isEmpty()) {
        for (const QJsonValue &vj : tools) {
            auto fj = vj.toObject();
            if (fj.value("type").toString() == "function") {
                response["function"] = QJsonValue(QString::fromUtf8(QJsonDocument(fj.value("function").toObject()).toJson(QJsonDocument::Compact)));
                break;
            }
        }
    }

    return response;
}

QSharedPointer<ErnieChat> ErnieFunctionCalling::ernieChat() const
{
    return qSharedPointerCast<ErnieChat>(chatIfs);
}
