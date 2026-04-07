// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doubaovision.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QMimeType>
#include <QEventLoop>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

using namespace doubao;

// Static constants
const QStringList DouBaoVision::SUPPORTED_FORMATS = {"jpg", "jpeg", "png", "gif", "bmp", "webp"};
const int DouBaoVision::MAX_IMAGE_SIZE = 10 * 1024 * 1024; // 10MB
const int DouBaoVision::REQUEST_TIMEOUT = 30000; // 30 seconds

DouBaoVision::DouBaoVision(const QString &model, QObject *parent)
    : ImageRecognitionInterface(model, parent)
    , networkManager(nullptr)
{
    networkManager = new QNetworkAccessManager(this);
    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    
    connect(timeoutTimer, &QTimer::timeout, this, &DouBaoVision::onRequestTimeout);
    
    qCInfo(logModelCenter) << "DouBaoVision created with model:" << model;
}

DouBaoVision::~DouBaoVision()
{
    terminate();
    qCInfo(logModelCenter) << "DouBaoVision destroyed";
}

void DouBaoVision::setApiKey(const QString &apiKey)
{
    this->apiKey = apiKey;
    qCDebug(logModelCenter) << "DouBaoVision API key set";
}

void DouBaoVision::setApiUrl(const QString &apiUrl)
{
    this->apiUrl = apiUrl;
    qCInfo(logModelCenter) << "DouBaoVision API URL set to:" << apiUrl;
}

void DouBaoVision::setMaxTokens(int maxTokens)
{
    this->maxTokens = maxTokens;
    qCDebug(logModelCenter) << "DouBaoVision max tokens set to:" << maxTokens;
}

void DouBaoVision::setTemperature(double temperature)
{
    this->temperature = temperature;
    qCDebug(logModelCenter) << "DouBaoVision temperature set to:" << temperature;
}

QJsonObject DouBaoVision::recognizeImage(const QList<VisionMessage> &messages, const QVariantHash &params)
{
    if (apiKey.isEmpty() || apiUrl.isEmpty()) {
        qCWarning(logModelCenter) << "API key or URL not configured for DouBaoVision";
        QJsonObject error;
        error["error"] = true;
        error["error_code"] = -1;
        error["error_message"] = "API key or URL not configured";
        return error;
    }
    
    if (messages.isEmpty()) {
        qCWarning(logModelCenter) << "Empty messages provided to DouBaoVision";
        QJsonObject error;
        error["error"] = true;
        error["error_code"] = -2;
        error["error_message"] = "No messages provided";
        return error;
    }
    
    qCInfo(logModelCenter) << "Starting synchronous image recognition with" << messages.size() << "messages";
    
    // Build request
    QJsonObject request = buildRequest(messages, params);
    QByteArray requestData = QJsonDocument(request).toJson();
    
    // Create network request
    QNetworkRequest networkRequest(apiUrl);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    
    // Send synchronous request
    QNetworkReply *reply = networkManager->post(networkRequest, requestData);
    
    // Wait for response
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // Start timeout timer
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(REQUEST_TIMEOUT);
    
    loop.exec();
    
    QJsonObject result;
    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            result = parseResponse(responseData);
            qCInfo(logModelCenter) << "Synchronous image recognition completed successfully";
        } else {
            qCWarning(logModelCenter) << "Network error in image recognition:" << reply->errorString();
            result["error"] = true;
            result["error_code"] = reply->error();
            result["error_message"] = reply->errorString();
        }
    } else {
        qCWarning(logModelCenter) << "Request timeout in image recognition";
        result["error"] = true;
        result["error_code"] = -3;
        result["error_message"] = "Request timeout";
        reply->abort();
    }
    
    reply->deleteLater();
    return result;
}

QString DouBaoVision::startImageRecognition(const QList<VisionMessage> &messages, const QVariantHash &params)
{
    if (apiKey.isEmpty() || apiUrl.isEmpty()) {
        qCWarning(logModelCenter) << "API key or URL not configured for DouBaoVision";
        return QString();
    }
    
    if (messages.isEmpty()) {
        qCWarning(logModelCenter) << "Empty messages provided to DouBaoVision";
        return QString();
    }
    
    QString sessionId = createSessionId();
    qCInfo(logModelCenter) << "Starting asynchronous image recognition session:" << sessionId;
    
    // Build request
    QJsonObject request = buildRequest(messages, params);
    QByteArray requestData = QJsonDocument(request).toJson();
    
    // Create network request
    QNetworkRequest networkRequest(apiUrl);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkRequest.setRawHeader("Authorization", QString("Bearer %1").arg(apiKey).toUtf8());
    
    // Send asynchronous request
    QNetworkReply *reply = networkManager->post(networkRequest, requestData);
    activeRequests[reply] = sessionId;
    
    connect(reply, &QNetworkReply::finished, this, &DouBaoVision::onNetworkReplyFinished);
    
    // Start timeout timer
    timeoutTimer->start(REQUEST_TIMEOUT);
    
    return sessionId;
}

void DouBaoVision::terminate()
{
    qCInfo(logModelCenter) << "Terminating DouBaoVision";
    
    // Stop timeout timer
    if (timeoutTimer && timeoutTimer->isActive()) {
        timeoutTimer->stop();
    }
    
    // Abort all active requests
    for (auto it = activeRequests.begin(); it != activeRequests.end(); ++it) {
        QNetworkReply *reply = it.key();
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    activeRequests.clear();
}

QStringList DouBaoVision::getSupportedImageFormats() const
{
    return SUPPORTED_FORMATS;
}

int DouBaoVision::getMaxImageSize() const
{
    return MAX_IMAGE_SIZE;
}

void DouBaoVision::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QString sessionId = activeRequests.value(reply);
    activeRequests.remove(reply);
    
    if (sessionId.isEmpty()) {
        qCWarning(logModelCenter) << "Unknown reply finished in DouBaoVision";
        reply->deleteLater();
        return;
    }
    
    // Stop timeout timer if this was the last request
    if (activeRequests.isEmpty() && timeoutTimer->isActive()) {
        timeoutTimer->stop();
    }
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonObject result = parseResponse(responseData);
        
        if (result.contains("error") && result["error"].toBool()) {
            emit recognitionError(sessionId, result["error_code"].toInt(), result["error_message"].toString());
        } else {
            QString content = result["content"].toString();
            emit recognitionResult(sessionId, content);
            emit recognitionCompleted(sessionId, content);
        }
        
        qCInfo(logModelCenter) << "Asynchronous image recognition completed for session:" << sessionId;
    } else {
        qCWarning(logModelCenter) << "Network error in async image recognition:" << reply->errorString();
        emit recognitionError(sessionId, reply->error(), reply->errorString());
    }
    
    reply->deleteLater();
}

void DouBaoVision::onRequestTimeout()
{
    qCWarning(logModelCenter) << "Request timeout in DouBaoVision";
    
    // Abort all active requests
    for (auto it = activeRequests.begin(); it != activeRequests.end(); ++it) {
        QNetworkReply *reply = it.key();
        QString sessionId = it.value();
        
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
        
        emit recognitionError(sessionId, -3, "Request timeout");
    }
    
    activeRequests.clear();
}

QJsonObject DouBaoVision::buildRequest(const QList<VisionMessage> &messages, const QVariantHash &params)
{
    QJsonObject request;
    request["model"] = modelName;
    request["max_tokens"] = params.value("max_tokens", maxTokens).toInt();
    request["temperature"] = params.value("temperature", temperature).toDouble();
    
    QJsonArray messageArray;
    for (const VisionMessage &msg : messages) {
        QJsonObject message;
        message["role"] = "user";
        
        QJsonArray contentArray;
        
        // Add text content if provided
        if (!msg.text.isEmpty()) {
            QJsonObject textContent;
            textContent["type"] = "text";
            textContent["text"] = msg.text;
            contentArray.append(textContent);
        }
        
        // Add image content
        QJsonObject imageContent;
        imageContent["type"] = "image_url";
        
        QJsonObject imageUrl;
        if (msg.imageType == "url" && !msg.imageUrl.isEmpty()) {
            imageUrl["url"] = msg.imageUrl;
        } else if (msg.imageType == "base64" && !msg.imageData.isEmpty()) {
            QString base64Data = QString("data:image/%1;base64,%2").arg(msg.mimeType, QString::fromUtf8(msg.imageData));
            imageUrl["url"] = base64Data;
        }
        
        imageContent["image_url"] = imageUrl;
        contentArray.append(imageContent);
        
        message["content"] = contentArray;
        messageArray.append(message);
    }
    
    request["messages"] = messageArray;
    
    qCDebug(logModelCenter) << "Built DouBaoVision request with" << messageArray.size() << "messages";
    return request;
}

QJsonObject DouBaoVision::parseResponse(const QByteArray &data)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qCWarning(logModelCenter) << "JSON parse error in DouBaoVision response:" << parseError.errorString();
        QJsonObject error;
        error["error"] = true;
        error["error_code"] = -4;
        error["error_message"] = QString("JSON parse error: %1").arg(parseError.errorString());
        return error;
    }
    
    QJsonObject response = doc.object();
    
    // Check for API error
    if (response.contains("error")) {
        QJsonObject apiError = response["error"].toObject();
        QJsonObject error;
        error["error"] = true;
        error["error_code"] = apiError["code"].toInt();
        error["error_message"] = apiError["message"].toString();
        return error;
    }
    
    // Extract content from choices
    QJsonArray choices = response["choices"].toArray();
    if (choices.isEmpty()) {
        QJsonObject error;
        error["error"] = true;
        error["error_code"] = -5;
        error["error_message"] = "No choices in response";
        return error;
    }
    
    QJsonObject firstChoice = choices[0].toObject();
    QJsonObject message = firstChoice["message"].toObject();
    QString content = message["content"].toString();
    
    QJsonObject result;
    result["error"] = false;
    result["content"] = content;
    result["usage"] = response["usage"];
    
    return result;
}

QString DouBaoVision::createSessionId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
