// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONBASE_H
#define SESSIONBASE_H

#include "aidaemon_global.h"
#include "common/aitypes.h"
#include "taskmanager/taskmanager.h"

#include <QDBusContext>
#include <QObject>
#include <QMutex>
#include <QSet>

AIDAEMON_BEGIN_NAMESPACE

class SessionManager;
class ModelCenter;

class SessionBase : public QObject, public QDBusContext
{
    Q_OBJECT
    
public:
    explicit SessionBase(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    virtual ~SessionBase();
    
    // Core session interface
    QString getSessionId() const { return sessionId; }
    virtual AITypes::ServiceType getServiceType() const = 0;
    
    // Simple lifecycle
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
    
    // Task management integration
    TaskManager* getTaskManager() const;
    
    // Model center integration
    ModelCenter* getModelCenter() const;
    
signals:
    void sessionFinished(const QString& sessionId);
    
protected:
    // Helper for subclasses to submit tasks
    void submitTask(const TaskManager::Task& task);
    
    // Task completion handlers - can be overridden by subclasses
    virtual void onTaskCompleted(quint64 taskId, const QVariant& result);
    virtual void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage);
    
    // Error handling
    void reportError(const QString& error);
    
    QString sessionId;
    SessionManager* sessionManager = nullptr;
    
    // Simple task tracking
    QSet<uint64_t> activeTasks;
    mutable QMutex taskMutex;
    
    // Helper for subclasses
    void cleanupTasks();
};

AIDAEMON_END_NAMESPACE

#endif // SESSIONBASE_H 
