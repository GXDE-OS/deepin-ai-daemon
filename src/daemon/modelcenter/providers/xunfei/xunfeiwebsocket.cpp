// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xunfeiwebsocket.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QUrlQuery>
#include <QUrl>
#include <QUuid>
#include <QLoggingCategory>
#include <QLocale>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

XunfeiWebSocket::XunfeiWebSocket(QObject *parent)
    : QObject(parent)
    , domain("iat")
    , language("zh_cn")
    , accent("mandarin")
{
    webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    
    connect(webSocket, &QWebSocket::connected, this, &XunfeiWebSocket::onConnected);
    connect(webSocket, &QWebSocket::disconnected, this, &XunfeiWebSocket::onDisconnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &XunfeiWebSocket::onTextMessageReceived);
    connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &XunfeiWebSocket::onError);
    connect(webSocket, &QWebSocket::sslErrors, this, &XunfeiWebSocket::onSslErrors);
}

XunfeiWebSocket::~XunfeiWebSocket()
{
    if (webSocket && webSocket->state() == QAbstractSocket::ConnectedState) {
        webSocket->close();
    }
}

void XunfeiWebSocket::setAppId(const QString &appId)
{
    this->appId = appId;
}

void XunfeiWebSocket::setApiKey(const QString &apiKey)
{
    this->apiKey = apiKey;
}

void XunfeiWebSocket::setApiSecret(const QString &apiSecret)
{
    this->apiSecret = apiSecret;
}

void XunfeiWebSocket::setDomain(const QString &domain)
{
    this->domain = domain;
}

void XunfeiWebSocket::setLanguage(const QString &language)
{
    this->language = language;
}

void XunfeiWebSocket::setAccent(const QString &accent)
{
    this->accent = accent;
}

void XunfeiWebSocket::connectToServer()
{
    if (appId.isEmpty() || apiKey.isEmpty() || apiSecret.isEmpty()) {
        emit error(-1, "Missing required credentials");
        return;
    }
    
    QString url = generateAuthUrl();
    qCDebug(logModelCenter) << "Connecting to Xunfei WebSocket:" << url;
    
    currentSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    currentRecognitionResult.clear();
    
    webSocket->open(QUrl(url));
}

void XunfeiWebSocket::disconnectFromServer()
{
    if (webSocket) {
        qCDebug(logModelCenter) << "Disconnecting WebSocket and resetting state";
        webSocket->close();
        
        // Reset recognition state
        currentRecognitionResult.clear();
        currentSessionId.clear();
    }
}

bool XunfeiWebSocket::isConnected() const
{
    return webSocket && webSocket->state() == QAbstractSocket::ConnectedState;
}

bool XunfeiWebSocket::isConnecting() const
{
    return webSocket && webSocket->state() == QAbstractSocket::ConnectingState;
}

void XunfeiWebSocket::sendFirstFrame(const QByteArray &audioData)
{
    if (!isConnected()) {
        emit error(-1, "WebSocket not connected");
        return;
    }
    
    QJsonObject message = createAudioMessage(audioData, STATUS_FIRST_FRAME);
    
    // Add business parameters for first frame
    QJsonObject business;
    business["domain"] = domain;
    business["language"] = language;
    business["accent"] = accent;
    business["vinfo"] = 1;
    business["vad_eos"] = 10000;
    message["business"] = business;
    
    // Add common parameters for first frame
    QJsonObject common;
    common["app_id"] = appId;
    message["common"] = common;
    
    QJsonDocument doc(message);
    webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void XunfeiWebSocket::sendContinueFrame(const QByteArray &audioData)
{
    if (!isConnected()) {
        emit error(-1, "WebSocket not connected");
        return;
    }
    
    QJsonObject message = createAudioMessage(audioData, STATUS_CONTINUE_FRAME);
    QJsonDocument doc(message);
    webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

void XunfeiWebSocket::sendLastFrame(const QByteArray &audioData)
{
    if (!isConnected()) {
        emit error(-1, "WebSocket not connected");
        return;
    }
    
    QJsonObject message = createAudioMessage(audioData, STATUS_LAST_FRAME);
    QJsonDocument doc(message);
    webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}

QString XunfeiWebSocket::sessionId() const
{
    return currentSessionId;
}

void XunfeiWebSocket::onConnected()
{
    qCDebug(logModelCenter) << "Xunfei WebSocket connected successfully";
    emit connected();
}

void XunfeiWebSocket::onDisconnected()
{
    qCDebug(logModelCenter) << "Xunfei WebSocket disconnected";
    emit disconnected();
}

void XunfeiWebSocket::onTextMessageReceived(const QString &message)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(logModelCenter) << "Failed to parse WebSocket message:" << error.errorString();
        return;
    }
    
    processRecognitionMessage(doc.object());
}

void XunfeiWebSocket::onError(QAbstractSocket::SocketError error)
{
    QString errorString = webSocket->errorString();
    qCWarning(logModelCenter) << "WebSocket error:" << error << errorString;
    emit this->error(static_cast<int>(error), errorString);
}

void XunfeiWebSocket::onSslErrors(const QList<QSslError> &errors)
{
    for (const QSslError &error : errors) {
        qCWarning(logModelCenter) << "SSL error:" << error.errorString();
    }
    // Ignore SSL errors for now (similar to Python demo)
    webSocket->ignoreSslErrors();
}

QString XunfeiWebSocket::generateAuthUrl()
{
    // Generate RFC1123 format timestamp (force English locale)
    QDateTime now = QDateTime::currentDateTimeUtc();
    QLocale englishLocale(QLocale::English, QLocale::UnitedStates);
    QString date = englishLocale.toString(now, "ddd, dd MMM yyyy hh:mm:ss") + " GMT";
    
    // Create signature origin string
    QString host = "ws-api.xfyun.cn";
    QString signatureOrigin = QString("host: %1\ndate: %2\nGET /v2/iat HTTP/1.1")
                             .arg(host, date);
    
    // Generate HMAC-SHA256 signature
    QByteArray signature = QMessageAuthenticationCode::hash(
        signatureOrigin.toUtf8(), 
        apiSecret.toUtf8(), 
        QCryptographicHash::Sha256
    );
    QString signatureBase64 = signature.toBase64();
    
    // Create authorization string
    QString authorization = QString("api_key=\"%1\", algorithm=\"hmac-sha256\", "
                                   "headers=\"host date request-line\", signature=\"%2\"")
                           .arg(apiKey, signatureBase64);
    QString authorizationBase64 = authorization.toUtf8().toBase64();
    
    // Build URL with query parameters
    QUrl url("wss://ws-api.xfyun.cn/v2/iat");
    QUrlQuery query;
    query.addQueryItem("authorization", authorizationBase64);
    query.addQueryItem("date", date);
    query.addQueryItem("host", host);
    url.setQuery(query);
    
    return url.toString();
}

QJsonObject XunfeiWebSocket::createAudioMessage(const QByteArray &audioData, int status)
{
    QJsonObject message;
    QJsonObject data;
    
    data["status"] = status;
    data["format"] = "audio/L16;rate=16000";
    data["encoding"] = "raw";
    data["audio"] = QString::fromUtf8(audioData.toBase64());
    
    message["data"] = data;
    return message;
}

void XunfeiWebSocket::processRecognitionMessage(const QJsonObject &message)
{
    int code = message["code"].toInt();
    QString sid = message["sid"].toString();
    
    if (code != 0) {
        QString errorMsg = message["message"].toString();
        qCWarning(logModelCenter) << "Recognition error - sid:" << sid 
                                  << "error:" << errorMsg << "code:" << code;
        emit error(code, errorMsg);
        return;
    }
    
    // Extract recognition result
    QJsonObject data = message["data"].toObject();
    QJsonObject result = data["result"].toObject();
    QJsonArray ws = result["ws"].toArray();
    
    QString resultText;
    for (const QJsonValue &wsValue : ws) {
        QJsonObject wsObj = wsValue.toObject();
        QJsonArray cw = wsObj["cw"].toArray();
        for (const QJsonValue &cwValue : cw) {
            QJsonObject cwObj = cwValue.toObject();
            resultText += cwObj["w"].toString();
        }
    }
    
    if (!resultText.isEmpty()) {
        qCDebug(logModelCenter) << "Recognition result:" << resultText;
        currentRecognitionResult += resultText;
        emit recognitionPartialResult(resultText);
    }
    
    // Check if this is the final result
    int status = data["status"].toInt();
    if (status == 2) { // Final result
        emit recognitionCompleted(currentRecognitionResult);
    }
}
