// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xunfeittswebsocket.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QDateTime>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

XunfeiTTSWebSocket::XunfeiTTSWebSocket(QObject *parent)
    : QObject(parent)
    , voice("x4_yezi")
    , audioFormat("raw")
    , textEncoding("utf8")
{
    webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    
    // Connect signals
    connect(webSocket, &QWebSocket::connected, this, &XunfeiTTSWebSocket::onConnected);
    connect(webSocket, &QWebSocket::disconnected, this, &XunfeiTTSWebSocket::onDisconnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &XunfeiTTSWebSocket::onTextMessageReceived);
    connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &XunfeiTTSWebSocket::onError);
    connect(webSocket, &QWebSocket::sslErrors, this, &XunfeiTTSWebSocket::onSslErrors);
    
    // Configure SSL
    QSslConfiguration sslConfig = webSocket->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    webSocket->setSslConfiguration(sslConfig);
}

XunfeiTTSWebSocket::~XunfeiTTSWebSocket()
{
    if (webSocket) {
        webSocket->close();
    }
}

void XunfeiTTSWebSocket::setAppId(const QString &appId)
{
    this->appId = appId;
}

void XunfeiTTSWebSocket::setApiKey(const QString &apiKey)
{
    this->apiKey = apiKey;
}

void XunfeiTTSWebSocket::setApiSecret(const QString &apiSecret)
{
    this->apiSecret = apiSecret;
}

void XunfeiTTSWebSocket::setVoice(const QString &voice)
{
    this->voice = voice;
}

void XunfeiTTSWebSocket::setAudioFormat(const QString &audioFormat)
{
    this->audioFormat = audioFormat;
}

void XunfeiTTSWebSocket::setTextEncoding(const QString &textEncoding)
{
    this->textEncoding = textEncoding;
}

void XunfeiTTSWebSocket::connectToServer()
{
    if (appId.isEmpty() || apiKey.isEmpty() || apiSecret.isEmpty()) {
        qCWarning(logModelCenter) << "Cannot connect: missing credentials";
        return;
    }
    
    QString url = generateAuthUrl();
    qCInfo(logModelCenter) << "Connecting to Xunfei TTS server:" << url;
    
    webSocket->open(QUrl(url));
}

void XunfeiTTSWebSocket::disconnectFromServer()
{
    if (webSocket) {
        qCDebug(logModelCenter) << "Disconnecting from Xunfei TTS server";
        webSocket->close();
        
        // Reset synthesis state
        currentAudioData.clear();
        currentSessionId.clear();
    }
}

bool XunfeiTTSWebSocket::isConnected() const
{
    return webSocket && webSocket->state() == QAbstractSocket::ConnectedState;
}

bool XunfeiTTSWebSocket::isConnecting() const
{
    return webSocket && webSocket->state() == QAbstractSocket::ConnectingState;
}

void XunfeiTTSWebSocket::synthesizeText(const QString &text)
{
    if (!isConnected()) {
        qCWarning(logModelCenter) << "Cannot synthesize text: not connected";
        return;
    }
    
    currentAudioData.clear();
    currentSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QJsonObject message = createSynthesisMessage(text);
    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    
    qCInfo(logModelCenter) << "Sending synthesis request for text:" << text.left(50) << "...";
    webSocket->sendTextMessage(jsonString);
}

QString XunfeiTTSWebSocket::sessionId() const
{
    return currentSessionId;
}

void XunfeiTTSWebSocket::onConnected()
{
    qCInfo(logModelCenter) << "Connected to Xunfei TTS server";
    emit connected();
}

void XunfeiTTSWebSocket::onDisconnected()
{
    qCInfo(logModelCenter) << "Disconnected from Xunfei TTS server";
    emit disconnected();
}

void XunfeiTTSWebSocket::onTextMessageReceived(const QString &message)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qCWarning(logModelCenter) << "Failed to parse TTS response:" << error.errorString();
        return;
    }
    
    QJsonObject jsonObj = doc.object();
    processSynthesisMessage(jsonObj);
}

void XunfeiTTSWebSocket::onError(QAbstractSocket::SocketError error)
{
    QString errorString = webSocket->errorString();
    qCWarning(logModelCenter) << "WebSocket error:" << error << errorString;
    emit this->error(static_cast<int>(error), errorString);
}

void XunfeiTTSWebSocket::onSslErrors(const QList<QSslError> &errors)
{
    for (const QSslError &error : errors) {
        qCWarning(logModelCenter) << "SSL error:" << error.errorString();
    }
}

QString XunfeiTTSWebSocket::generateAuthUrl()
{
    // Generate RFC1123 format timestamp (same as Python's format_date_time)
    QDateTime now = QDateTime::currentDateTimeUtc();
    
    // Format date manually to ensure RFC1123 compliance
    static const char* weekDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    int dayOfWeek = now.date().dayOfWeek() - 1; // Qt uses 1-7, we need 0-6
    if (dayOfWeek < 0) dayOfWeek = 6; // Sunday becomes 6
    
    QString date = QString("%1, %2 %3 %4 %5:%6:%7 GMT")
                   .arg(weekDays[dayOfWeek])
                   .arg(now.date().day(), 2, 10, QChar('0'))
                   .arg(months[now.date().month() - 1])
                   .arg(now.date().year())
                   .arg(now.time().hour(), 2, 10, QChar('0'))
                   .arg(now.time().minute(), 2, 10, QChar('0'))
                   .arg(now.time().second(), 2, 10, QChar('0'));
    
    // Create signature origin string
    QString signatureOrigin = QString("host: ws-api.xfyun.cn\n"
                                     "date: %1\n"
                                     "GET /v2/tts HTTP/1.1").arg(date);
    
    // Create HMAC-SHA256 signature
    QMessageAuthenticationCode hmac(QCryptographicHash::Sha256);
    hmac.setKey(apiSecret.toUtf8());
    hmac.addData(signatureOrigin.toUtf8());
    QByteArray signatureSha = hmac.result();
    QString signatureShaBase64 = signatureSha.toBase64();
    
    // Create authorization string
    QString authorizationOrigin = QString("api_key=\"%1\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%2\"")
                                    .arg(apiKey, signatureShaBase64);
    QString authorization = authorizationOrigin.toUtf8().toBase64();
    
    // Build URL with authentication parameters
    QUrl url("wss://tts-api.xfyun.cn/v2/tts");
    QUrlQuery query;
    query.addQueryItem("authorization", authorization);
    query.addQueryItem("date", date);
    query.addQueryItem("host", "ws-api.xfyun.cn");
    url.setQuery(query);
    
    return url.toString();
}

QJsonObject XunfeiTTSWebSocket::createSynthesisMessage(const QString &text)
{
    // Common parameters
    QJsonObject common;
    common["app_id"] = appId;
    
    // Business parameters
    QJsonObject business;
    business["aue"] = audioFormat;
    business["auf"] = "audio/L16;rate=16000";
    business["vcn"] = voice;
    business["tte"] = textEncoding;
    
    // Data parameters
    QJsonObject data;
    data["status"] = STATUS_LAST_FRAME;  // For TTS, we send all text at once
    data["text"] = QString::fromUtf8(text.toUtf8().toBase64());
    
    // Complete message
    QJsonObject message;
    message["common"] = common;
    message["business"] = business;
    message["data"] = data;
    
    return message;
}

void XunfeiTTSWebSocket::processSynthesisMessage(const QJsonObject &message)
{
    int code = message["code"].toInt();
    QString sid = message["sid"].toString();
    
    if (code != 0) {
        QString errorMsg = message["message"].toString();
        qCWarning(logModelCenter) << "TTS synthesis error - sid:" << sid << "code:" << code << "message:" << errorMsg;
        emit error(code, errorMsg);
        return;
    }
    
    QJsonObject data = message["data"].toObject();
    int status = data["status"].toInt();
    QString audioBase64 = data["audio"].toString();
    
    // Decode audio data
    QByteArray audioData = QByteArray::fromBase64(audioBase64.toUtf8());
    currentAudioData.append(audioData);
    
    qCDebug(logModelCenter) << "Received TTS audio data - sid:" << sid << "status:" << status << "size:" << audioData.size();
    
    // Emit synthesis result
    emit synthesisResult(audioData);
    
    // Check if synthesis is complete
    if (status == STATUS_LAST_FRAME) {
        qCInfo(logModelCenter) << "TTS synthesis completed - sid:" << sid << "total size:" << currentAudioData.size();
        emit synthesisCompleted(currentAudioData);
        webSocket->close();
    }
}
