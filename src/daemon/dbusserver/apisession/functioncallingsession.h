// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FUNCTIONCALLINGSESSION_H
#define FUNCTIONCALLINGSESSION_H

#include "sessionbase.h"
#include "taskmanager/taskmanager.h"

#include <QDBusMessage>
#include <QMap>
#include <QVariantMap>
#include <QJsonArray>

AIDAEMON_BEGIN_NAMESPACE

class FunctionCallingSession : public SessionBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.Session.FunctionCalling")
    
public:
    explicit FunctionCallingSession(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    ~FunctionCallingSession();
    
    // SessionBase implementation
    AITypes::ServiceType getServiceType() const override { return AITypes::ServiceType::FunctionCalling; }
    bool initialize() override;
    void cleanup() override;
    
    // Function calling state
    bool isGenerating() const { return generating; }
    
public slots:
    // D-Bus interface methods - business logic resides here
    void Terminate();
    QString Parse(const QString &content, const QString &functions, const QString &params);
    
signals:
    void FunctionResult(const QString &result);
    void ParseError(int error, const QString &message);
    
private slots:
    void onTaskCompleted(quint64 taskId, const QVariant& result);
    void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage);
    
private:
    // Task creation helpers
    TaskManager::Task createFunctionCallingTask(const QString &content, const QString &functions, const QVariantMap &params);
    
    // Helper methods
    void handleFunctionCallingResult(quint64 taskId, bool success, const QVariant& result);
    void sendFunctionCallingResponse(const QDBusMessage& message, const QString& response);
    void sendFunctionCallingError(const QDBusMessage& message, int errorCode, const QString& errorMsg);
    
    // Validation helpers
    bool validateFunctionDefinitions(const QString& functions, QString& errorMsg);
    QJsonArray parseFunctionDefinitions(const QString& functions);
    
    // Utility methods
    void complete();
    void completeInternal(); // Internal version without locking
    
    // Session state
    bool generating = false;
    
    // Task to D-Bus message mapping for response handling
    QMap<quint64, QDBusMessage> pendingMessages; // taskId -> dbusMessage
    
    mutable QMutex stateMutex;
};

AIDAEMON_END_NAMESPACE

#endif // FUNCTIONCALLINGSESSION_H 
