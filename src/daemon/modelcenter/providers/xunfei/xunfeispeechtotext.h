// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEISPEECHTOTEXT_H
#define XUNFEISPEECHTOTEXT_H

#include "modelcenter/interfaces/speechtotextinterface.h"
#include "xunfeiwebsocket.h"
#include "xunfeiaudioprocessor.h"

#include <QObject>
#include <QMutex>
#include <QTimer>
#include <QEventLoop>
#include <QHash>

AIDAEMON_BEGIN_NAMESPACE

class XunfeiSpeechToText : public SpeechToTextInterface
{
    Q_OBJECT
public:
    explicit XunfeiSpeechToText(const QString &model, QObject *parent = nullptr);
    ~XunfeiSpeechToText();
    
    // Configuration methods
    void setAppId(const QString &appId);
    void setApiKey(const QString &apiKey);
    void setApiSecret(const QString &apiSecret);
    void setDomain(const QString &domain);
    void setLanguage(const QString &language);
    void setAccent(const QString &accent);
    
    // SpeechToTextInterface implementation
    QJsonObject recognizeFile(const QString &audioFile, 
                             const QVariantHash &params = {}) override;
    QString startStreamRecognition(const QVariantHash &params = {}) override;
    bool sendAudioData(const QString &sessionId, 
                      const QByteArray &audioData) override;
    QJsonObject endStreamRecognition(const QString &sessionId) override;
    
    // Information methods
    QStringList getSupportedFormats() const override;
    
    void terminate() override;
    
private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(int errorCode, const QString &errorMessage);
    void onRecognitionPartialResult(const QString &partialText);
    void onRecognitionCompleted(const QString &finalText);
    void onFrameTimer();
    
private:
    struct StreamSession {
        QString sessionId;
        QList<QByteArray> audioFrames;
        int currentFrameIndex;
        QString result;
        bool isActive;
        QEventLoop *eventLoop;
        QByteArray audioBuffer;  // Audio buffer for streaming data
        
        StreamSession() : currentFrameIndex(0), isActive(false), eventLoop(nullptr) {}
    };
    
    void processAudioFile(const QString &audioFile);
    void sendNextFrame(StreamSession &session);
    void processAudioBuffer(StreamSession &session);  // Process audio buffer for streaming
    QJsonObject createResultObject(const QString &text, int errorCode = 0, const QString &errorMessage = "");
    
private:
    XunfeiWebSocket *webSocket;
    XunfeiAudioProcessor *audioProcessor;
    QTimer *frameTimer;
    
    QString appId;
    QString apiKey;
    QString apiSecret;
    QString domain;
    QString language;
    QString accent;
    
    // Current session state (single session only)
    StreamSession currentSession;
    QString currentSessionId;
    bool processingAudioBuffer;  // Flag to prevent re-entrant processing
    
    // File recognition state
    QString fileRecognitionResult;
    QEventLoop *fileEventLoop;
    int lastErrorCode;
    QString lastErrorMessage;
    

};

AIDAEMON_END_NAMESPACE

#endif // XUNFEISPEECHTOTEXT_H 
