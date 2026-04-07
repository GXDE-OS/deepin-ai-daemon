// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPEECHTOTEXTSESSION_H
#define SPEECHTOTEXTSESSION_H

#include "sessionbase.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelcenter_define.h"
#include "modelcenter/interfaces/speechtotextinterface.h"

#include <QDBusMessage>
#include <QMap>
#include <QVariantMap>
#include <QMutex>

AIDAEMON_BEGIN_NAMESPACE

class SpeechToTextSession : public SessionBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.Session.SpeechToText")
    
public:
    explicit SpeechToTextSession(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    ~SpeechToTextSession() override;
    
    // SessionBase implementation
    AITypes::ServiceType getServiceType() const override { return AITypes::ServiceType::SpeechToText; }
    bool initialize() override;
    void cleanup() override;
    
    // Recognition state
    bool isRecognizing() const { return recognizing; }
    
public slots:
    // D-Bus interface methods - business logic resides here
    void terminate();
    QString recognizeFile(const QString &audioFile, const QString &params);
    QString startStreamRecognition(const QString &params);
    bool sendAudioData(const QString &streamSessionId, const QByteArray &audioData);
    QString endStreamRecognition(const QString &streamSessionId);
    
    // Information methods
    QStringList getSupportedFormats();
    
signals:
    // Stream recognition results
    void RecognitionResult(const QString &streamSessionId, const QString &text);
    void RecognitionPartialResult(const QString &streamSessionId, const QString &partialText);
    void RecognitionError(const QString &streamSessionId, int errorCode, const QString &errorMessage);
    void RecognitionCompleted(const QString &streamSessionId, const QString &finalText);
    
private slots:
    void onTaskCompleted(quint64 taskId, const QVariant& result) override;
    void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage) override;
    
    // Stream recognition interface signal handlers
    void onInterfaceRecognitionResult(const QString &sessionId, const QString &text);
    void onInterfaceRecognitionPartialResult(const QString &sessionId, const QString &partialText);
    void onInterfaceRecognitionError(const QString &sessionId, int errorCode, const QString &message);
    void onInterfaceRecognitionCompleted(const QString &sessionId, const QString &finalText);
    
private:
    // Task creation helpers
    TaskManager::Task createRecognitionTask(const QString &audioFile, const QVariantMap &params);
    TaskManager::Task createStreamRecognitionTask(const QString &streamSessionId, const QVariantMap &params);
    
    // Helper methods
    void handleRecognitionResult(quint64 taskId, bool success, const QVariant& result);
    void sendRecognitionResponse(const QDBusMessage& message, const QString& response);
    void sendRecognitionError(const QDBusMessage& message, int errorCode, const QString& errorMsg);
    
    // Stream session management
    struct StreamSession {
        QString streamSessionId;
        bool isActive = false;
        QByteArray audioBuffer;
        quint64 taskId = 0;
    };
    
    // State management
    bool recognizing = false;
    QMap<quint64, QDBusMessage> pendingMessages;
    QMap<quint64, bool> streamingTasks;
    QMap<QString, StreamSession> streamSessions;
    
    // Speech interface for stream recognition
    QSharedPointer<SpeechToTextInterface> currentSpeechInterface;
    
    mutable QMutex stateMutex;
};

AIDAEMON_END_NAMESPACE

#endif // SPEECHTOTEXTSESSION_H 
