// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXTTOSPEECHSESSION_H
#define TEXTTOSPEECHSESSION_H

#include "sessionbase.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelcenter_define.h"
#include "modelcenter/interfaces/texttospeechinterface.h"

#include <QDBusMessage>
#include <QMap>
#include <QVariantMap>
#include <QMutex>

AIDAEMON_BEGIN_NAMESPACE

class TextToSpeechSession : public SessionBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.Session.TextToSpeech")
    
public:
    explicit TextToSpeechSession(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    ~TextToSpeechSession() override;
    
    // SessionBase implementation
    AITypes::ServiceType getServiceType() const override { return AITypes::ServiceType::TextToSpeech; }
    bool initialize() override;
    void cleanup() override;
    
    // Synthesis state
    bool isSynthesizing() const { return synthesizing; }
    
public slots:
    // D-Bus interface methods - business logic resides here
    void terminate();
    QString startStreamSynthesis(const QString &text, const QString &params);
    QString endStreamSynthesis(const QString &streamSessionId);
    
    // Information methods
    QStringList getSupportedVoices();
    
signals:
    // Stream synthesis results
    void SynthesisResult(const QString &streamSessionId, const QByteArray &audioData);
    void SynthesisError(const QString &streamSessionId, int errorCode, const QString &errorMessage);
    void SynthesisCompleted(const QString &streamSessionId, const QByteArray &finalAudio);
    
private slots:
    void onTaskCompleted(quint64 taskId, const QVariant& result) override;
    void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage) override;
    
    // Stream synthesis interface signal handlers
    void onInterfaceSynthesisResult(const QString &sessionId, const QByteArray &audioData);
    void onInterfaceSynthesisError(const QString &sessionId, int errorCode, const QString &message);
    void onInterfaceSynthesisCompleted(const QString &sessionId, const QByteArray &finalAudio);
    
private:
    // Task creation helpers
    TaskManager::Task createStreamSynthesisTask(const QString &streamSessionId, const QVariantMap &params);
    
    // Helper methods
    void handleSynthesisResult(quint64 taskId, bool success, const QVariant& result);
    void sendSynthesisResponse(const QDBusMessage& message, const QString& response);
    void sendSynthesisError(const QDBusMessage& message, int errorCode, const QString& errorMsg);
    
    // Stream session management
    struct StreamSession {
        QString streamSessionId;
        bool isActive = false;
        QByteArray audioData;
        quint64 taskId = 0;
    };
    
    // State management
    bool synthesizing = false;
    QMap<quint64, QDBusMessage> pendingMessages;
    QMap<quint64, bool> streamingTasks;
    QMap<QString, StreamSession> streamSessions;
    
    // TTS interface for stream synthesis
    QSharedPointer<TextToSpeechInterface> currentTTSInterface;
    
    mutable QMutex stateMutex;
};

AIDAEMON_END_NAMESPACE

#endif // TEXTTOSPEECHSESSION_H
