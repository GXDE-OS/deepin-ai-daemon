// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "universalfunctioncallinginterface.h"
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

AIDAEMON_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

UniversalFunctionCallingInterface::UniversalFunctionCallingInterface(QSharedPointer<UniversalChatInterface> ifs, QObject *parent)
    : FunctionCallingInterface(qSharedPointerCast<ChatInterface>(ifs), parent)
{

}

QJsonObject UniversalFunctionCallingInterface::parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params)
{
    qCDebug(logModelCenter) << "Starting function parsing with content length:" << content.length();
    
    AIConversation conversion;
    conversion.addUserData(content);
    conversion.setFunctions(func);

    QJsonObject dataObject;
    dataObject.insert("model", model());
    dataObject.insert("messages", conversion.getConversions());
    dataObject.insert("stream", false);

    if (!conversion.getFunctions().isEmpty()) {
        qCDebug(logModelCenter) << "Processing" << conversion.getFunctions().size() << "functions";
        QJsonArray tools;
        for (const QJsonValue &func : conversion.getFunctions()) {
            QJsonObject funcObj;
            funcObj.insert("type", "function");
            funcObj.insert("function", func);
            tools.append(funcObj);
        }
        dataObject.insert("tools", tools);
    }

    ModelUtils::addFilteredParams(dataObject, params);

    QJsonDocument jsonDocument(dataObject);
    const QByteArray &sendData = jsonDocument.toJson(QJsonDocument::Compact);

    QHttpMultiPart *multipart = nullptr;

    auto chat = chatInterface();
    QSharedPointer<HttpAccessmanager> httpAccessManager(new HttpAccessmanager(chat->apiKey()));
    QNetworkRequest req = httpAccessManager->baseNetWorkRequest(chat->apiUrl());

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

    HttpEventLoop loop(reply, "UniversalChatInterface::functionCalling");
    loop.setHttpOutTime(chat->timeOut());
    connect(chat.data(), &UniversalChatInterface::eventLoopAbort, &loop, &HttpEventLoop::abortReply);
    loop.exec();

    QNetworkReply::NetworkError netReplyError;
    bool isAuthError = httpAccessManager->isAuthenticationRequiredError();
    if (isAuthError) {
        qCWarning(logModelCenter) << "Authentication error occurred during function call";
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

    chat->setLastError(netReplyError);
    conversion.update(loop.getHttpResult());

    if (netReplyError == QNetworkReply::NoError) {
        qCDebug(logModelCenter) << "Function call completed successfully";
    }

    QJsonObject response;
    response["content"] = conversion.getLastResponse();

    QJsonArray tools = conversion.getLastTools();
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

QSharedPointer<UniversalChatInterface> UniversalFunctionCallingInterface::chatInterface() const
{
    return qSharedPointerCast<UniversalChatInterface>(chatIfs);
}
