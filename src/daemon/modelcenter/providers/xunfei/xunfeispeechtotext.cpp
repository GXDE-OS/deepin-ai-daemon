// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xunfeispeechtotext.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QThread>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

XunfeiSpeechToText::XunfeiSpeechToText(const QString &model, QObject *parent)
    : SpeechToTextInterface(model, parent)
    , webSocket(nullptr)
    , audioProcessor(nullptr)
    , frameTimer(nullptr)
    , domain("iat")
    , language("zh_cn")
    , accent("mandarin")
    , fileEventLoop(nullptr)
    , lastErrorCode(0)
    , processingAudioBuffer(false)
{
    webSocket = new XunfeiWebSocket(this);
    audioProcessor = new XunfeiAudioProcessor(this);
    frameTimer = new QTimer(this);

    // Connect WebSocket signals
    connect(webSocket, &XunfeiWebSocket::connected,
            this, &XunfeiSpeechToText::onWebSocketConnected);
    connect(webSocket, &XunfeiWebSocket::disconnected,
            this, &XunfeiSpeechToText::onWebSocketDisconnected);
    connect(webSocket, &XunfeiWebSocket::error,
            this, &XunfeiSpeechToText::onWebSocketError);
    connect(webSocket, &XunfeiWebSocket::recognitionPartialResult,
            this, &XunfeiSpeechToText::onRecognitionPartialResult);
    connect(webSocket, &XunfeiWebSocket::recognitionCompleted,
            this, &XunfeiSpeechToText::onRecognitionCompleted);

    // Connect frame timer
    connect(frameTimer, &QTimer::timeout, this, &XunfeiSpeechToText::onFrameTimer);
    frameTimer->setInterval(XunfeiAudioProcessor::getFrameInterval());
}

XunfeiSpeechToText::~XunfeiSpeechToText()
{
    terminate();
}

void XunfeiSpeechToText::setAppId(const QString &appId)
{
    this->appId = appId;
    if (webSocket) {
        webSocket->setAppId(appId);
    }
}

void XunfeiSpeechToText::setApiKey(const QString &apiKey)
{
    this->apiKey = apiKey;
    if (webSocket) {
        webSocket->setApiKey(apiKey);
    }
}

void XunfeiSpeechToText::setApiSecret(const QString &apiSecret)
{
    this->apiSecret = apiSecret;
    if (webSocket) {
        webSocket->setApiSecret(apiSecret);
    }
}

void XunfeiSpeechToText::setDomain(const QString &domain)
{
    this->domain = domain;
    if (webSocket) {
        webSocket->setDomain(domain);
    }
}

void XunfeiSpeechToText::setLanguage(const QString &language)
{
    this->language = language;
    if (webSocket) {
        webSocket->setLanguage(language);
    }
}

void XunfeiSpeechToText::setAccent(const QString &accent)
{
    this->accent = accent;
    if (webSocket) {
        webSocket->setAccent(accent);
    }
}

QJsonObject XunfeiSpeechToText::recognizeFile(const QString &audioFile, const QVariantHash &params)
{
    Q_UNUSED(params)

    qCInfo(logModelCenter) << "Starting file recognition for:" << audioFile;

    if (!XunfeiAudioProcessor::isValidAudioFile(audioFile)) {
        return createResultObject("", -1, "Invalid audio file");
    }

    if (!XunfeiAudioProcessor::isSupportedFormat(audioFile)) {
        return createResultObject("", -2, "Unsupported audio format");
    }

    // Reset state
    fileRecognitionResult.clear();
    lastErrorCode = 0;
    lastErrorMessage.clear();

    // Process audio file in a separate thread to avoid blocking
    QEventLoop eventLoop;
    fileEventLoop = &eventLoop;

    // Start processing
    processAudioFile(audioFile);

    // Wait for completion
    eventLoop.exec();
    fileEventLoop = nullptr;

    return createResultObject(fileRecognitionResult, lastErrorCode, lastErrorMessage);
}

QString XunfeiSpeechToText::startStreamRecognition(const QVariantHash &params)
{
    Q_UNUSED(params)

    QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Initialize current session
    currentSession.sessionId = sessionId;
    currentSession.isActive = true;
    currentSession.currentFrameIndex = 0;
    currentSession.result.clear();
    currentSession.audioFrames.clear();
    currentSession.audioBuffer.clear();
    currentSession.eventLoop = nullptr;

    currentSessionId = sessionId;

    qCInfo(logModelCenter) << "Started stream recognition session:" << sessionId;

    // Establish WebSocket connection immediately, following Python implementation's on_open logic
    if (!webSocket->isConnected() && !webSocket->isConnecting()) {
        qCDebug(logModelCenter) << "Establishing WebSocket connection for session:" << sessionId;
        webSocket->connectToServer();
    } else if (webSocket->isConnected()) {
        qCDebug(logModelCenter) << "WebSocket already connected, ready for audio data";
    }

    return sessionId;
}

bool XunfeiSpeechToText::sendAudioData(const QString &sessionId, const QByteArray &audioData)
{
    if (sessionId != currentSessionId) {
        qCWarning(logModelCenter) << "Invalid session ID:" << sessionId << "expected:" << currentSessionId;
        return false;
    }

    if (!currentSession.isActive) {
        qCWarning(logModelCenter) << "Session is not active:" << sessionId;
        return false;
    }

    // Add audio data to buffer, following Python implementation's audio buffering logic
    currentSession.audioBuffer.append(audioData);
    
    qCDebug(logModelCenter) << "Added" << audioData.size() << "bytes to session:" << sessionId 
                            << "total buffer:" << currentSession.audioBuffer.size();

    // Process audio data immediately if WebSocket is connected
    if (webSocket->isConnected()) {
        processAudioBuffer(currentSession);
    }

    return true;
}

QJsonObject XunfeiSpeechToText::endStreamRecognition(const QString &sessionId)
{
    if (sessionId != currentSessionId) {
        return createResultObject("", -1, "Invalid session ID");
    }

    currentSession.isActive = false;

    // Send remaining audio data
    if (webSocket->isConnected()) {
        // Process remaining audio buffer
        if (!currentSession.audioBuffer.isEmpty()) {
            qCDebug(logModelCenter) << "Sending remaining" << currentSession.audioBuffer.size() << "bytes";
            webSocket->sendLastFrame(currentSession.audioBuffer);
            currentSession.audioBuffer.clear();
        } else {
            // Send empty final frame to indicate end
            webSocket->sendLastFrame(QByteArray());
        }

        // Wait for completion
        QEventLoop eventLoop;
        currentSession.eventLoop = &eventLoop;
        eventLoop.exec();
        currentSession.eventLoop = nullptr;
    }

    QString result = currentSession.result;
    
    // Clear current session
    currentSessionId.clear();
    currentSession = StreamSession();

    qCInfo(logModelCenter) << "Ended stream recognition session:" << sessionId << "result:" << result;

    return createResultObject(result);
}

QStringList XunfeiSpeechToText::getSupportedFormats() const
{
    // Return the audio formats supported by Xunfei Speech Recognition service
    // These formats are processed by XunfeiAudioProcessor before sending to the service
    QStringList formats;
    formats << "pcm"     // PCM raw audio (16kHz, 16-bit, mono)
            << "wav"     // WAVE audio files
            << "mp3"     // MP3 compressed audio
            << "flac"    // FLAC lossless audio
            << "aac"     // AAC compressed audio
            << "ogg"     // OGG Vorbis audio
            << "m4a";    // MPEG-4 Audio
    
    qCDebug(logModelCenter) << "Xunfei Speech Recognition supported formats:" << formats;
    return formats;
}

void XunfeiSpeechToText::terminate()
{
    if (frameTimer) {
        frameTimer->stop();
    }

    if (webSocket && webSocket->isConnected()) {
        webSocket->disconnectFromServer();
    }

    // Clean up current session
    if (currentSession.eventLoop) {
        currentSession.eventLoop->quit();
    }
    currentSessionId.clear();
    currentSession = StreamSession();

    if (fileEventLoop) {
        fileEventLoop->quit();
    }
}

void XunfeiSpeechToText::onWebSocketConnected()
{
    qCDebug(logModelCenter) << "WebSocket connected, ready for audio processing";

    // Process buffered audio data immediately if current session has data
    if (!currentSessionId.isEmpty() && !currentSession.audioBuffer.isEmpty()) {
        qCDebug(logModelCenter) << "Processing buffered audio data for session:" << currentSessionId;
        processAudioBuffer(currentSession);
    }
}

void XunfeiSpeechToText::onWebSocketDisconnected()
{
    qCDebug(logModelCenter) << "WebSocket disconnected";
    frameTimer->stop();
}

void XunfeiSpeechToText::onWebSocketError(int errorCode, const QString &errorMessage)
{
    qCWarning(logModelCenter) << "WebSocket error:" << errorCode << errorMessage;

    lastErrorCode = errorCode;
    lastErrorMessage = errorMessage;

    // Notify waiting event loops
    if (fileEventLoop) {
        fileEventLoop->quit();
    }

    // Emit error signal for current session
    if (!currentSessionId.isEmpty()) {
        if (currentSession.eventLoop) {
            currentSession.eventLoop->quit();
        }
        emit recognitionError(currentSessionId, errorCode, errorMessage);
    }
    
    // Clean up current session and reset state after error
    qCDebug(logModelCenter) << "Cleaning up session after WebSocket error";
    currentSessionId.clear();
    currentSession = StreamSession();
    
    // Stop frame timer to prevent further processing
    if (frameTimer) {
        frameTimer->stop();
    }
    
    // Ensure WebSocket is disconnected to allow clean reconnection
    if (webSocket && webSocket->isConnected()) {
        qCDebug(logModelCenter) << "Disconnecting WebSocket after error for clean state";
        webSocket->disconnectFromServer();
    }
}

void XunfeiSpeechToText::onRecognitionPartialResult(const QString &partialText)
{
    qCDebug(logModelCenter) << "Partial result:" << partialText;

    if (!currentSessionId.isEmpty()) {
        emit recognitionPartialResult(currentSessionId, partialText);
    }
}

void XunfeiSpeechToText::onRecognitionCompleted(const QString &finalText)
{
    qCInfo(logModelCenter) << "Recognition completed:" << finalText;

    // Update file recognition result
    fileRecognitionResult = finalText;

    // Update current session result
    if (!currentSessionId.isEmpty()) {
        currentSession.result = finalText;
        emit recognitionCompleted(currentSessionId, finalText);

        if (currentSession.eventLoop) {
            currentSession.eventLoop->quit();
        }
    }

    // Quit file event loop if waiting
    if (fileEventLoop) {
        fileEventLoop->quit();
    }

    frameTimer->stop();
}

void XunfeiSpeechToText::onFrameTimer()
{
    // Timer mainly ensures audio data is sent timely, but main logic is in processAudioBuffer
    if (currentSessionId.isEmpty()) {
        return;
    }

    if (!currentSession.audioBuffer.isEmpty()) {
        processAudioBuffer(currentSession);
    }
}

void XunfeiSpeechToText::processAudioFile(const QString &audioFile)
{
    // Load and process audio file
    QByteArray audioData = audioProcessor->loadAudioFile(audioFile);
    if (audioData.isEmpty()) {
        lastErrorCode = -3;
        lastErrorMessage = "Failed to load audio file";
        if (fileEventLoop) {
            fileEventLoop->quit();
        }
        return;
    }

    // Split into frames
    QList<QByteArray> frames = audioProcessor->splitToFrames(audioData);
    if (frames.isEmpty()) {
        lastErrorCode = -4;
        lastErrorMessage = "No audio frames generated";
        if (fileEventLoop) {
            fileEventLoop->quit();
        }
        return;
    }

    // Create temporary session for file processing
    QString sessionId = "file_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    StreamSession session;
    session.sessionId = sessionId;
    session.audioFrames = frames;
    session.isActive = true;

    currentSession = session;
    currentSessionId = sessionId;

    // Connect and start processing
    if (!webSocket->isConnected()) {
        webSocket->connectToServer();
    } else {
        sendNextFrame(session);
    }
}

void XunfeiSpeechToText::processAudioBuffer(StreamSession &session)
{
    if (!webSocket->isConnected()) {
        return;
    }

    // Prevent re-entrant processing to avoid deadlock
    if (processingAudioBuffer) {
        qCDebug(logModelCenter) << "processAudioBuffer already in progress, skipping";
        return;
    }

    processingAudioBuffer = true;

    // Audio buffer processing logic following Python implementation
    while (session.audioBuffer.size() >= XunfeiAudioProcessor::getDefaultFrameSize()) {
        QByteArray frameData = session.audioBuffer.left(XunfeiAudioProcessor::getDefaultFrameSize());
        session.audioBuffer.remove(0, XunfeiAudioProcessor::getDefaultFrameSize());

        if (session.currentFrameIndex == 0) {
            // First frame
            webSocket->sendFirstFrame(frameData);
            frameTimer->start();
            qCDebug(logModelCenter) << "Sent first frame:" << frameData.size() << "bytes";
        } else {
            // Continue frame
            webSocket->sendContinueFrame(frameData);
            qCDebug(logModelCenter) << "Sent continue frame:" << frameData.size() << "bytes";
        }

        session.currentFrameIndex++;
    }

    processingAudioBuffer = false;
}

void XunfeiSpeechToText::sendNextFrame(StreamSession &session)
{
    if (!webSocket->isConnected()) {
        return;
    }

    if (session.currentFrameIndex >= session.audioFrames.size()) {
        // Send final empty frame
        webSocket->sendLastFrame(QByteArray());
        return;
    }

    QByteArray frameData = session.audioFrames[session.currentFrameIndex];

    if (session.currentFrameIndex == 0) {
        // First frame
        webSocket->sendFirstFrame(frameData);
        frameTimer->start();
    } else if (session.currentFrameIndex == session.audioFrames.size() - 1) {
        // Last frame
        webSocket->sendLastFrame(frameData);
        frameTimer->stop();
    } else {
        // Continue frame
        webSocket->sendContinueFrame(frameData);
    }

    session.currentFrameIndex++;

    qCDebug(logModelCenter) << "Sent frame" << session.currentFrameIndex
                            << "of" << session.audioFrames.size();
}

QJsonObject XunfeiSpeechToText::createResultObject(const QString &text, int errorCode, const QString &errorMessage)
{
    QJsonObject result;
    result["text"] = text;
    result["error_code"] = errorCode;
    result["error_message"] = errorMessage;
    result["success"] = (errorCode == 0);

    return result;
}
