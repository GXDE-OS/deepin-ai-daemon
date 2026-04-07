// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionbase.h"
#include "sessionmanager/sessionmanager.h"
#include "taskmanager/taskmanager.h"

#include <QLoggingCategory>
#include <QMutexLocker>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

SessionBase::SessionBase(const QString &sessionId, SessionManager *manager, QObject *parent)
    : QObject(parent)
    , QDBusContext()
    , sessionId(sessionId)
    , sessionManager(manager)
{
    qCDebug(logDBusServer) << "SessionBase created:" << this->sessionId;
    
    // Connect TaskManager signals
    auto* taskManager = getTaskManager();
    if (taskManager) {
        connect(taskManager, &TaskManager::taskCompleted,
                this, &SessionBase::onTaskCompleted);
        connect(taskManager, &TaskManager::taskFailed,
                this, &SessionBase::onTaskFailed);
        qCDebug(logDBusServer) << "TaskManager signals connected for session:" << this->sessionId;
    } else {
        qCWarning(logDBusServer) << "TaskManager not available for session:" << this->sessionId;
    }
}

SessionBase::~SessionBase()
{
    // Don't call pure virtual cleanup() in destructor
    // cleanupTasks() will handle task cleanup
    cleanupTasks();
    qCDebug(logDBusServer) << "SessionBase destroyed:" << sessionId;
}

TaskManager* SessionBase::getTaskManager() const
{
    if (sessionManager) {
        return sessionManager->getTaskManager();
    }
    return nullptr;
}

ModelCenter* SessionBase::getModelCenter() const
{
    if (sessionManager) {
        return sessionManager->getModelCenter();
    }
    return nullptr;
}

void SessionBase::submitTask(const TaskManager::Task& task)
{
    auto* taskManager = getTaskManager();
    if (!taskManager) {
        qCWarning(logDBusServer) << "TaskManager not available for session:" << sessionId;
        return;
    }
    
    QMutexLocker locker(&taskMutex);
    activeTasks.insert(task.taskId);
    
    qCDebug(logDBusServer) << "Submitting task:" << task.taskId 
                           << "for session:" << sessionId;
    
    taskManager->addTask(task);
}

void SessionBase::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    QMutexLocker locker(&taskMutex);
    activeTasks.remove(taskId);
    
    qCDebug(logDBusServer) << "Task completed:" << taskId 
                           << "for session:" << sessionId;
    
    // Default implementation does nothing - subclasses should override
    Q_UNUSED(result)
}

void SessionBase::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    QMutexLocker locker(&taskMutex);
    activeTasks.remove(taskId);
    
    qCWarning(logDBusServer) << "Task failed:" << taskId 
                            << "error:" << errorCode << errorMessage
                            << "for session:" << sessionId;
    
    // Default implementation does nothing - subclasses should override
}

void SessionBase::reportError(const QString& error)
{
    qCWarning(logDBusServer) << "Session error:" << sessionId << error;
    
    // Emit finished signal to trigger cleanup
    emit sessionFinished(sessionId);
}

void SessionBase::cleanupTasks()
{
    QMutexLocker locker(&taskMutex);
    
    if (!activeTasks.isEmpty()) {
        qCInfo(logDBusServer) << "Cleaning up" << activeTasks.size() 
                              << "active tasks for session:" << sessionId;
        
        auto* taskManager = getTaskManager();
        if (taskManager) {
            for (uint64_t taskId : activeTasks) {
                taskManager->cancelTask(taskId);
            }
        }
        
        activeTasks.clear();
    }
} 