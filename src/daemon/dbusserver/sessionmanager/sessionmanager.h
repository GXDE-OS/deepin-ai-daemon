// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include "aidaemon_global.h"
#include "common/aitypes.h"

#include <QObject>
#include <QMutex>
#include <QSharedPointer>
#include <QMap>
#include <QMultiMap>

AIDAEMON_BEGIN_NAMESPACE

class SessionBase;
class TaskManager;
class ModelCenter;

class SessionManager : public QObject
{
    Q_OBJECT
public:
    struct SessionInfo {
        QString sessionId;
        AITypes::ServiceType serviceType;
        QString clientId;
    };
    
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager();
    
    // Core session management
    QString createSession(AITypes::ServiceType type, const QString& clientId);
    bool destroySession(const QString& sessionId);
    QSharedPointer<SessionBase> getSession(const QString& sessionId);
    
    // Simple monitoring - only provide basic information
    QList<SessionInfo> getAllSessions() const;
    int getSessionCount() const;
    
    // Task manager integration
    void setTaskManager(TaskManager* taskManager);
    TaskManager* getTaskManager() const;
    
    // Model center integration
    void setModelCenter(ModelCenter* modelCenter);
    ModelCenter* getModelCenter() const;
    
signals:
    void sessionCreated(const QString& sessionId);
    void sessionDestroyed(const QString& sessionId);
    
private:
    QString generateSessionId() const;
    QSharedPointer<SessionBase> createSessionInstance(AITypes::ServiceType type, const QString& sessionId);
    
    // Simple session storage
    QMap<QString, SessionInfo> sessions;
    QMap<QString, QSharedPointer<SessionBase>> activeSessions;
    
    // Client tracking for cleanup on disconnect ONLY
    QMultiMap<QString, QString> clientSessions; // clientId -> sessionIds
    
    mutable QMutex sessionMutex;
    TaskManager* taskManager = nullptr;
    ModelCenter* modelCenter = nullptr;
};

AIDAEMON_END_NAMESPACE

#endif // SESSIONMANAGER_H 