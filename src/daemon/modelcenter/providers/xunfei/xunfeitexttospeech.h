// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEITEXTTOSPEECH_H
#define XUNFEITEXTTOSPEECH_H

#include "modelcenter/interfaces/texttospeechinterface.h"
#include "xunfeittswebsocket.h"

#include <QObject>
#include <QMutex>
#include <QEventLoop>
#include <QHash>

AIDAEMON_BEGIN_NAMESPACE

class XunfeiTextToSpeech : public TextToSpeechInterface
{
    Q_OBJECT
public:
    explicit XunfeiTextToSpeech(const QString &model, QObject *parent = nullptr);
    ~XunfeiTextToSpeech();
    
    // Configuration methods
    void setAppId(const QString &appId);
    void setApiKey(const QString &apiKey);
    void setApiSecret(const QString &apiSecret);
    void setVoice(const QString &voice);
    void setAudioFormat(const QString &audioFormat);
    void setTextEncoding(const QString &textEncoding);

    QString startStreamSynthesis(const QString &text,
                                const QVariantHash &params = {}) override;
    QJsonObject endStreamSynthesis(const QString &sessionId) override;
    
    // Information methods
    QStringList getSupportedVoices() const override;
    
    void terminate() override;
    
private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(int errorCode, const QString &errorMessage);
    void onSynthesisResult(const QByteArray &audioData);
    void onSynthesisCompleted(const QByteArray &finalAudio);
    
private:
    struct SynthesisSession {
        QString sessionId;
        QString text;
        QByteArray audioData;
        bool isActive;
        QEventLoop *eventLoop;
        
        SynthesisSession() : isActive(false), eventLoop(nullptr) {}
    };
    
    void processSynthesisRequest(const QString &text, const QVariantHash &params);
    QJsonObject createResultObject(const QByteArray &audioData, int errorCode = 0, const QString &errorMessage = "");
    
private:
    XunfeiTTSWebSocket *webSocket = nullptr;
    
    QString appId;
    QString apiKey;
    QString apiSecret;
    QString voice;
    QString audioFormat;
    QString textEncoding;
    
    // Session management
    QHash<QString, SynthesisSession> synthesisSessions;
    QString currentSessionId;
    QMutex sessionMutex;
    
    // File synthesis state
    QByteArray fileSynthesisResult;
    int lastErrorCode = 0;
    QString lastErrorMessage;
};

AIDAEMON_END_NAMESPACE

#endif // XUNFEITEXTTOSPEECH_H
