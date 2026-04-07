// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXTCHATSESSION_H
#define TEXTCHATSESSION_H

#include "sessionbase.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelcenter_define.h"

#include <QDBusMessage>
#include <QMap>
#include <QVariantMap>

AIDAEMON_BEGIN_NAMESPACE

class TextChatSession : public SessionBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.Session.Chat")
    
public:
    explicit TextChatSession(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    ~TextChatSession() override;
    
    // SessionBase implementation
    AITypes::ServiceType getServiceType() const override { return AITypes::ServiceType::Chat; }
    bool initialize() override;
    void cleanup() override;
    
    // Chat state
    bool isGenerating() const { return generating; }
    
public slots:
    // D-Bus interface methods - business logic resides here
    void terminate();
    QString chat(const QString &content, const QString &params);
    int streamChat(const QString &content, const QString &params);
    
signals:
    void StreamOutput(const QString &text);
    void StreamFinished(int error, const QString &content);
    
private slots:
    void onTaskCompleted(quint64 taskId, const QVariant& result) override;
    void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage) override;
    
private:
    // Task creation helpers
    TaskManager::Task createChatTask(const QString &content, const QVariantMap &params, bool stream);
    
    // Helper methods
    void handleChatResult(quint64 taskId, bool success, const QVariant& result);
    void sendChatResponse(const QDBusMessage& message, const QString& response);
    void sendChatError(const QDBusMessage& message, int errorCode, const QString& errorMsg);
    
    // Utility methods
    QList<ChatHistory> extractHistoryFromParams(const QVariantMap &params) const;
    void complete();
    void completeInternal(); // Internal version without locking
    
    // Session state
    bool generating = false;
    
    // Task to D-Bus message mapping for response handling
    QMap<quint64, QDBusMessage> pendingMessages; // taskId -> dbusMessage
    QMap<quint64, bool> streamingTasks;           // taskId -> isStreaming
    
    mutable QMutex stateMutex;
};

AIDAEMON_END_NAMESPACE

#endif // TEXTCHATSESSION_H 
