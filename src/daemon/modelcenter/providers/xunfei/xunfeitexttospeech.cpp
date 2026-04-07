// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xunfeitexttospeech.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QThread>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

XunfeiTextToSpeech::XunfeiTextToSpeech(const QString &model, QObject *parent)
    : TextToSpeechInterface(model, parent)
    , webSocket(nullptr)
    , voice("x4_yezi")
    , audioFormat("raw")
    , textEncoding("utf8")
{
    webSocket = new XunfeiTTSWebSocket(this);

    // Connect WebSocket signals
    connect(webSocket, &XunfeiTTSWebSocket::connected,
            this, &XunfeiTextToSpeech::onWebSocketConnected);
    connect(webSocket, &XunfeiTTSWebSocket::disconnected,
            this, &XunfeiTextToSpeech::onWebSocketDisconnected);
    connect(webSocket, &XunfeiTTSWebSocket::error,
            this, &XunfeiTextToSpeech::onWebSocketError);
    connect(webSocket, &XunfeiTTSWebSocket::synthesisResult,
            this, &XunfeiTextToSpeech::onSynthesisResult);
    connect(webSocket, &XunfeiTTSWebSocket::synthesisCompleted,
            this, &XunfeiTextToSpeech::onSynthesisCompleted);
}

XunfeiTextToSpeech::~XunfeiTextToSpeech()
{
    terminate();
}

void XunfeiTextToSpeech::setAppId(const QString &appId)
{
    this->appId = appId;
    if (webSocket) {
        webSocket->setAppId(appId);
    }
}

void XunfeiTextToSpeech::setApiKey(const QString &apiKey)
{
    this->apiKey = apiKey;
    if (webSocket) {
        webSocket->setApiKey(apiKey);
    }
}

void XunfeiTextToSpeech::setApiSecret(const QString &apiSecret)
{
    this->apiSecret = apiSecret;
    if (webSocket) {
        webSocket->setApiSecret(apiSecret);
    }
}

void XunfeiTextToSpeech::setVoice(const QString &voice)
{
    this->voice = voice;
    if (webSocket) {
        webSocket->setVoice(voice);
    }
}

void XunfeiTextToSpeech::setAudioFormat(const QString &audioFormat)
{
    this->audioFormat = audioFormat;
    if (webSocket) {
        webSocket->setAudioFormat(audioFormat);
    }
}

void XunfeiTextToSpeech::setTextEncoding(const QString &textEncoding)
{
    this->textEncoding = textEncoding;
    if (webSocket) {
        webSocket->setTextEncoding(textEncoding);
    }
}

QString XunfeiTextToSpeech::startStreamSynthesis(const QString &text, const QVariantHash &params)
{
    Q_UNUSED(params)
    
    if (text.isEmpty()) {
        qCWarning(logModelCenter) << "Cannot start stream synthesis: empty text";
        return QString();
    }
    
    QMutexLocker locker(&sessionMutex);
    
    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    SynthesisSession session;
    session.sessionId = sessionId;
    session.text = text;
    session.isActive = true;
    session.audioData.clear();
    
    synthesisSessions[sessionId] = session;
    currentSessionId = sessionId;
    
    qCInfo(logModelCenter) << "Started stream synthesis session:" << sessionId << "for text:" << text.left(50) << "...";
    
    // Connect to server and start synthesis
    if (!webSocket->isConnected() && !webSocket->isConnecting()) {
        webSocket->connectToServer();
    } else if (webSocket->isConnected()) {
        webSocket->synthesizeText(text);
    }
    
    return sessionId;
}

QJsonObject XunfeiTextToSpeech::endStreamSynthesis(const QString &sessionId)
{
    QMutexLocker locker(&sessionMutex);
    
    auto it = synthesisSessions.find(sessionId);
    if (it == synthesisSessions.end()) {
        qCWarning(logModelCenter) << "Stream synthesis session not found:" << sessionId;
        return createResultObject(QByteArray(), -1, "Session not found");
    }
    
    SynthesisSession &session = it.value();
    session.isActive = false;
    
    qCInfo(logModelCenter) << "Ended stream synthesis session:" << sessionId;
    
    return createResultObject(session.audioData, 0, "");
}

QStringList XunfeiTextToSpeech::getSupportedVoices() const
{
    // Return the voices supported by Xunfei TTS service
    // These are the standard voices available in Xunfei TTS
    QStringList voices;
    voices << "x4_yezi"
           << "x4_xiaoyan"
           << "x4_xiaofeng"
           << "x4_xiaomei"
           << "x4_xiaoyun";
    
    qCDebug(logModelCenter) << "Xunfei TTS supported voices:" << voices;
    return voices;
}

void XunfeiTextToSpeech::terminate()
{
    QMutexLocker locker(&sessionMutex);
    
    // Disconnect WebSocket
    if (webSocket) {
        webSocket->disconnectFromServer();
    }
    
    // Clear all sessions
    synthesisSessions.clear();
    currentSessionId.clear();
    
    fileSynthesisResult.clear();
    lastErrorCode = 0;
    lastErrorMessage.clear();
    
    qCInfo(logModelCenter) << "TTS terminated";
}

void XunfeiTextToSpeech::onWebSocketConnected()
{
    qCInfo(logModelCenter) << "WebSocket connected for TTS";
    
    // If we have a current session, start synthesis
    if (!currentSessionId.isEmpty()) {
        auto it = synthesisSessions.find(currentSessionId);
        if (it != synthesisSessions.end()) {
            webSocket->synthesizeText(it.value().text);
        }
    }
}

void XunfeiTextToSpeech::onWebSocketDisconnected()
{
    qCInfo(logModelCenter) << "WebSocket disconnected for TTS";
}

void XunfeiTextToSpeech::onWebSocketError(int errorCode, const QString &errorMessage)
{
    qCWarning(logModelCenter) << "WebSocket error for TTS:" << errorCode << errorMessage;
    
    lastErrorCode = errorCode;
    lastErrorMessage = errorMessage;
    
    // Emit error for stream synthesis
    if (!currentSessionId.isEmpty()) {
        emit synthesisError(currentSessionId, errorCode, errorMessage);
    }
}

void XunfeiTextToSpeech::onSynthesisResult(const QByteArray &audioData)
{
    qCDebug(logModelCenter) << "Received TTS synthesis result, size:" << audioData.size();
    
    // Add to file synthesis result
    fileSynthesisResult.append(audioData);
    
    // Add to stream synthesis session
    if (!currentSessionId.isEmpty()) {
        auto it = synthesisSessions.find(currentSessionId);
        if (it != synthesisSessions.end()) {
            it.value().audioData.append(audioData);
            emit synthesisResult(currentSessionId, audioData);
        }
    }
}

void XunfeiTextToSpeech::onSynthesisCompleted(const QByteArray &finalAudio)
{
    qCInfo(logModelCenter) << "TTS synthesis completed, total size:" << finalAudio.size();
    
    // Update file synthesis result
    fileSynthesisResult = finalAudio;
    
    // Update stream synthesis session
    if (!currentSessionId.isEmpty()) {
        auto it = synthesisSessions.find(currentSessionId);
        if (it != synthesisSessions.end()) {
            it.value().audioData = finalAudio;
            it.value().isActive = false;
            emit synthesisCompleted(currentSessionId, finalAudio);
        }
    }
}

void XunfeiTextToSpeech::processSynthesisRequest(const QString &text, const QVariantHash &params)
{
    Q_UNUSED(params)
    
    // Connect to server if not connected
    if (!webSocket->isConnected() && !webSocket->isConnecting()) {
        webSocket->connectToServer();
    } else if (webSocket->isConnected()) {
        // Start synthesis immediately
        webSocket->synthesizeText(text);
    }
}

QJsonObject XunfeiTextToSpeech::createResultObject(const QByteArray &audioData, int errorCode, const QString &errorMessage)
{
    QJsonObject result;
    
    if (errorCode == 0) {
        result["success"] = true;
        result["error_code"] = 0;
        result["error_message"] = "";
        result["audio_data"] = QString::fromUtf8(audioData.toBase64());
        result["audio_size"] = audioData.size();
    } else {
        result["success"] = false;
        result["error_code"] = errorCode;
        result["error_message"] = errorMessage;
        result["audio_data"] = "";
        result["audio_size"] = 0;
    }
    
    return result;
}
