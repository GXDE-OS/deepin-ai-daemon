// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionmanagerdbus.h"
#include "sessionmanager/sessionmanager.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusError>
#include <QLoggingCategory>
#include <QMutexLocker>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

SessionManagerDBus::SessionManagerDBus(SessionManager* sessionManager, QObject *parent)
    : QObject(parent)
    , QDBusContext()
    , sessionManager(sessionManager)
{
    // Connect to D-Bus owner changed signal for client disconnect cleanup
    auto connection = QDBusConnection::sessionBus();
    connection.connect("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                      "NameOwnerChanged", this, SLOT(onDBusOwnerChanged(QString, QString, QString)));
    
    // Connect to session manager signals
    connect(this->sessionManager, &SessionManager::sessionDestroyed, this, &SessionManagerDBus::onSessionDestroyed);
    
    qCInfo(logDBusServer) << "SessionManagerDBus initialized";
}

SessionManagerDBus::~SessionManagerDBus()
{
    qCInfo(logDBusServer) << "SessionManagerDBus destroyed";
}

QString SessionManagerDBus::CreateSession(const QString &type)
{
    QDBusMessage msg = message();
    QString clientId = msg.service();
    setDelayedReply(true);
    
    if (clientId.isEmpty()) {
        qCWarning(logDBusServer) << "Empty client ID for CreateSession request";
        QDBusConnection::sessionBus().send(msg.createErrorReply(QDBusError::InvalidArgs, "Empty client ID"));
        return QString();
    }
    
    AITypes::ServiceType serviceType = stringToServiceType(type);
    
    // Post creation to SessionManager's thread and reply asynchronously
    QMetaObject::invokeMethod(sessionManager, [this, msg, clientId, serviceType, type]() mutable {
        QString sessionId = sessionManager->createSession(serviceType, clientId);
        if (!sessionId.isEmpty()) {
            {
                QMutexLocker locker(&clientMutex);
                dbusClientSessions.insert(clientId, sessionId);
            }
            qCInfo(logDBusServer) << "Created session:" << sessionId
                                  << "type:" << type
                                  << "for D-Bus client:" << clientId;
            QDBusConnection::sessionBus().send(msg.createReply(sessionId));
        } else {
            qCWarning(logDBusServer) << "Failed to create session for type:" << type
                                     << "client:" << clientId;
            QDBusConnection::sessionBus().send(msg.createErrorReply(QDBusError::Failed, "Failed to create session"));
        }
    }, Qt::QueuedConnection);
    
    // Return immediately; actual reply is sent later
    return QString();
}

void SessionManagerDBus::DestroySession(const QString &sessionId)
{
    QDBusMessage msg = message();
    setDelayedReply(true);

    if (sessionId.isEmpty()) {
        qCWarning(logDBusServer) << "Empty session ID for DestroySession request";
        QDBusConnection::sessionBus().send(msg.createErrorReply(QDBusError::InvalidArgs, "Empty session ID"));
        return;
    }

    // Use queued call; send reply (void) when done
    QMetaObject::invokeMethod(sessionManager, [this, msg, sessionId]() mutable {
        const bool success = sessionManager->destroySession(sessionId);
        if (success) {
            qCInfo(logDBusServer) << "Destroyed session via D-Bus:" << sessionId;
            QDBusConnection::sessionBus().send(msg.createReply());
        } else {
            qCWarning(logDBusServer) << "Failed to destroy session:" << sessionId;
            QDBusConnection::sessionBus().send(msg.createErrorReply(QDBusError::Failed, "Failed to destroy session"));
        }
    }, Qt::QueuedConnection);
}

QStringList SessionManagerDBus::GetAllSessions()
{
    QDBusMessage msg = message();

    // Switch to delayed reply to avoid blocking the D-Bus thread
    setDelayedReply(true);

    QMetaObject::invokeMethod(sessionManager, [this, msg]() mutable {
        QStringList result;
        const auto sessions = sessionManager->getAllSessions();
        result.reserve(sessions.size());
        for (const auto &session : sessions) {
            result.append(session.sessionId);
        }
        qCDebug(logDBusServer) << "Returning" << result.size() << "sessions via D-Bus";
        QDBusConnection::sessionBus().send(msg.createReply(result));
    }, Qt::QueuedConnection);

    // Return immediately; actual reply is sent later
    return QStringList();
}

void SessionManagerDBus::onDBusOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    
    // Client disconnected - cleanup its sessions
    if (newOwner.isEmpty() && !name.isEmpty()) {
        cleanupClientSessions(name);
    }
}

void SessionManagerDBus::onSessionDestroyed(const QString& sessionId)
{
    // Remove session from D-Bus client tracking
    QMutexLocker locker(&clientMutex);
    
    for (auto it = dbusClientSessions.begin(); it != dbusClientSessions.end(); ++it) {
        if (it.value() == sessionId) {
            dbusClientSessions.erase(it);
            break;
        }
    }
}

AITypes::ServiceType SessionManagerDBus::stringToServiceType(const QString& typeStr) const
{
    return AITypes::stringToServiceType(typeStr);
}

void SessionManagerDBus::cleanupClientSessions(const QString& clientId)
{
    QMutexLocker locker(&clientMutex);
    
    QStringList sessionIds = dbusClientSessions.values(clientId);
    dbusClientSessions.remove(clientId);
    
    if (!sessionIds.isEmpty()) {
        qCInfo(logDBusServer) << "Cleaning up" << sessionIds.size() 
                              << "sessions for disconnected D-Bus client:" << clientId;
        
        // Release lock before calling SessionManager (avoid deadlock)
        locker.unlock();
        
        for (const QString& sessionId : sessionIds) {
            // Use QMetaObject::invokeMethod for thread-safe cross-thread call
            QMetaObject::invokeMethod(sessionManager, [this, sessionId]() {
                sessionManager->destroySession(sessionId);
            }, Qt::QueuedConnection); // Use QueuedConnection for cleanup (non-blocking)
        }
    }
} 
