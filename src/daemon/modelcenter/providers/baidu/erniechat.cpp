// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "erniechat.h"
#include "ernieconversation.h"
#include "modelcenter/http/httpaccessmanager.h"
#include "modelcenter/http/httpeventloop.h"
#include "modelcenter/modelutils.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QHttpMultiPart>
#include <QString>
#include <QUrlQuery>
#include <QMutexLocker>
#include <QLoggingCategory>

AIDAEMON_USE_NAMESPACE

using namespace ernie;

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

QString ErnieChat::accountID = "";
QString ErnieChat::accessToken = "";
QMutex ErnieChat::accessMtx = QMutex();

ErnieChat::ErnieChat(const QString &model, QObject *parent)
    : ChatInterface(model, parent)
{

}

InterfaceType ErnieChat::type()
{
    return Chat;
}

QJsonObject ErnieChat::generate(const QString &content, bool stream, const QList<ChatHistory> &history, const QVariantHash &params)
{
    QString urlPath = apiPath.isEmpty() ? modelUrl(model()) : apiPath;

    if (urlPath.isEmpty()) {
        qCWarning(logModelCenter) << "Empty URL path for model:" << model();
        return {};
    } else {
        qCDebug(logModelCenter) << "Preparing request to URL:" << urlPath;
    }

    ErnieConversation conversation;
    conversation.fromHistory(history);
    conversation.addUserData(content);

    QJsonObject dataObject;
    dataObject.insert("messages", conversation.getConversions());
    //dataObject.insert("temperature", qBound(0.0, temperature, 1.0));
    dataObject.insert("stream", stream);

    ModelUtils::addFilteredParams(dataObject, params);

    QJsonDocument jsonDocument(dataObject);
    const QByteArray &sendData = jsonDocument.toJson(QJsonDocument::Compact);

    QString accessToken = updateAccessToken(apiKey, apiSecret);
    if (accessToken.isEmpty()) {
        qCCritical(logModelCenter) << "Failed to get access token for API request";
        return {};
    }

    QUrl url(urlPath);
    QUrlQuery query;
    query.addQueryItem("access_token", accessToken);
    url.setQuery(query);

    QHttpMultiPart *multipart = nullptr;

    QSharedPointer<HttpAccessmanager> httpAccessManager(new HttpAccessmanager(apiKey));
    QNetworkRequest req = httpAccessManager->baseNetWorkRequest(url);

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

    HttpEventLoop loop(reply, "ErnieChat::generate");
    loop.setHttpOutTime(60 * 1000);
    connect(&loop, &HttpEventLoop::sigReadyRead, this, &ErnieChat::onReadDeltaContent);
    connect(this, &ErnieChat::eventLoopAbort, &loop, &HttpEventLoop::abortReply);
    loop.exec();

    QNetworkReply::NetworkError netReplyError;
    bool isAuthError = httpAccessManager->isAuthenticationRequiredError();
    if (isAuthError)
        netReplyError = QNetworkReply::NetworkError::AuthenticationRequiredError;
    else if (loop.getHttpStatusCode() == 429)
        netReplyError = QNetworkReply::NetworkError::InternalServerError;
    else
        netReplyError = loop.getNetWorkError();

    conversation.update(loop.getHttpResult());

    QJsonObject response;
    response["content"] = conversation.getLastResponse();

    return response;
//    QJsonObject resultObject = QJsonDocument::fromJson(baseresult.data).object();
//    if (resultObject.contains("error_code") && resultObject.value("error_code").toInt() > 0) {
//        int code = resultObject.value("error_code").toInt();
//        if (code == 4 || code == 18) {
//            baseresult.error = AIServer::ServerRateLimitError;
//        } else {
//            baseresult.error = AIServer::ContentAccessDenied;
//        }
//        baseresult.data = WXQFCodeTranslation::serverCodeTranlation(code, resultObject.value("error_msg").toString()).toUtf8();
//    } else if (baseresult.error != AIServer::NoError) {
//        // 这里正在请求过程中遇到网络错误，存在残留数据，需要清理掉
//        baseresult.data.clear();
//    }

//    if (baseresult.error != AIServer::NoError && baseresult.data.isEmpty()) {
//        baseresult.data = ServerCodeTranslation::serverCodeTranslation(baseresult.error, baseresult.errorString).toUtf8();
//    }

//    return qMakePair(baseresult.error, baseresult.data);
}

void ErnieChat::terminate()
{
    emit eventLoopAbort();
}

QString ErnieChat::modelUrl(const QString &model)
{
    QString ret;
    static QString base = "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop";

    if (model.compare("ERNIE-3.5-8K", Qt::CaseInsensitive) == 0) {
        ret = base + "/chat/completions";
    } else if (model.compare("ERNIE-4.0-8K", Qt::CaseInsensitive) == 0) {
        ret = base + "/chat/completions_pro";
    } else if (model.compare("ERNIE-4.0-8K-Latest", Qt::CaseInsensitive) == 0) {
        ret = base + "/chat/ernie-4.0-8k-latest";
    } else if (model.compare("ERNIE-4.0-Turbo-8K", Qt::CaseInsensitive) == 0) {
        ret = base + "/chat/ernie-4.0-turbo-8k";
    } else if (model.compare("ERNIE-Lite-8K", Qt::CaseInsensitive) == 0) {
        ret = base + "/chat/ernie-lite-8k";
    }

    return ret;
}

void ErnieChat::onReadDeltaContent(const QByteArray &content)
{
    if (content.isEmpty())
        return;

    QJsonObject deltacontent = deltaContent.parseContentString(QString::fromUtf8(content));

    if (deltacontent.contains("content"))
        emit sendDeltaContent(deltacontent.value("content").toString());
}

QString ErnieChat::updateAccessToken(const QString &key, const QString &secret)
{
    if (key.isEmpty() || secret.isEmpty()) {
        qCWarning(logModelCenter) << "Empty API key or secret provided";
        return "";
    }
    QString id = key + secret;
    if (!accessMtx.tryLock(5 * 1000)) {
        qCWarning(logModelCenter) << "Failed to acquire access token mutex within timeout";
        return "";
    }

    if (!accessToken.isEmpty() && accountID == id) {
        accessMtx.unlock();
        return accessToken;
    }

    QSharedPointer<HttpAccessmanager> manager(new HttpAccessmanager(""));
    QUrl url("https://aip.baidubce.com/oauth/2.0/token");
    QUrlQuery query;
    query.addQueryItem("grant_type", "client_credentials");
    query.addQueryItem("client_id", key);
    query.addQueryItem("client_secret", secret);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "text/event-stream");

    QNetworkReply *tokenReply = manager->post(request, "");
    HttpEventLoop loop(tokenReply, "generateAccessToken");
    loop.setHttpOutTime(15000);
    connect(this, &ErnieChat::eventLoopAbort, &loop, &HttpEventLoop::abortReply);
    loop.exec();

    if (loop.getNetWorkError() != QNetworkReply::NoError) {
        int netReplyError;
        bool isAuthError = manager->isAuthenticationRequiredError();
        if (isAuthError)
            netReplyError = QNetworkReply::NetworkError::AuthenticationRequiredError;
        else if (loop.getHttpStatusCode() == 429)
            netReplyError = QNetworkReply::NetworkError::InternalServerError;
        else
            netReplyError = loop.getNetWorkError();
        qCWarning(logModelCenter) << "Failed to update access token, network error:" << netReplyError;
        accessMtx.unlock();
        return "";
    } else {
        QJsonObject jsonObject = QJsonDocument::fromJson(loop.getHttpResult()).object();
        accessToken = jsonObject.value("access_token").toString();
        accountID = id;
    }

    accessMtx.unlock();
    return accessToken;
}
