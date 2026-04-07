// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionmanager.h"
#include "apisession/sessionbase.h"
#include "apisession/textchatsession.h"
#include "apisession/functioncallingsession.h"
#include "apisession/speechtotextsession.h"
#include "apisession/texttospeechsession.h"
#include "apisession/imagerecognitionsession.h"
#include "apisession/ocrsession.h"
#include "taskmanager/taskmanager.h"

#include <QUuid>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QDBusConnection>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    qCInfo(logDBusServer) << "SessionManager initialized";
}

SessionManager::~SessionManager()
{
    // Clean up all active sessions
    QMutexLocker locker(&sessionMutex);
    for (auto it = activeSessions.begin(); it != activeSessions.end(); ++it) {
        // Unregister from D-Bus
        QString dbusPath = QString("/org/deepin/ai/daemon/Session/%1").arg(it.key());
        QDBusConnection::sessionBus().unregisterObject(dbusPath);
        
        it.value()->cleanup();
    }
    activeSessions.clear();
    sessions.clear();
    clientSessions.clear();
    
    qCInfo(logDBusServer) << "SessionManager destroyed";
}

QString SessionManager::createSession(AITypes::ServiceType type, const QString& clientId)
{
    QMutexLocker locker(&sessionMutex);
    
    QString sessionId = generateSessionId();
    
    // Create session instance
    auto session = createSessionInstance(type, sessionId);
    if (!session) {
        qCWarning(logDBusServer) << "Failed to create session for type:" << static_cast<int>(type);
        return QString();
    }
    
    // Initialize session
    if (!session->initialize()) {
        qCWarning(logDBusServer) << "Failed to initialize session:" << sessionId;
        return QString();
    }
    
    // Register session as D-Bus object
    QString dbusPath = QString("/org/deepin/ai/daemon/Session/%1").arg(sessionId);
    auto connection = QDBusConnection::sessionBus();
    if (!connection.registerObject(dbusPath, session.data(), QDBusConnection::ExportAllContents)) {
        qCWarning(logDBusServer) << "Failed to register session D-Bus object:" << dbusPath;
        return QString();
    }
    
    // Store session info - minimal information
    SessionInfo info;
    info.sessionId = sessionId;
    info.serviceType = type;
    info.clientId = clientId;
    
    sessions[sessionId] = info;
    activeSessions[sessionId] = session;
    clientSessions.insert(clientId, sessionId);
    
    // Connect session finished signal for cleanup
    connect(session.data(), &SessionBase::sessionFinished, this, [this, sessionId]() {
        destroySession(sessionId);
    });
    
    emit sessionCreated(sessionId);
    
    qCInfo(logDBusServer) << "Created session:" << sessionId 
                          << "type:" << AITypes::serviceTypeToString(type)
                          << "for client:" << clientId
                          << "D-Bus path:" << dbusPath;
    return sessionId;
}

bool SessionManager::destroySession(const QString& sessionId)
{
    QMutexLocker locker(&sessionMutex);
    
    auto sessionIt = sessions.find(sessionId);
    if (sessionIt == sessions.end()) {
        qCWarning(logDBusServer) << "Session not found for destruction:" << sessionId;
        return false;
    }
    
    const SessionInfo& info = sessionIt.value();
    
    // Unregister from D-Bus
    QString dbusPath = QString("/org/deepin/ai/daemon/Session/%1").arg(sessionId);
    QDBusConnection::sessionBus().unregisterObject(dbusPath);
    
    // Cleanup session
    auto activeIt = activeSessions.find(sessionId);
    if (activeIt != activeSessions.end()) {
        activeIt.value()->cleanup();
        activeSessions.erase(activeIt);
    }
    
    // Remove tracking
    clientSessions.remove(info.clientId, sessionId);
    sessions.erase(sessionIt);
    
    emit sessionDestroyed(sessionId);
    
    qCInfo(logDBusServer) << "Destroyed session:" << sessionId << "D-Bus path:" << dbusPath;
    return true;
}

QSharedPointer<SessionBase> SessionManager::getSession(const QString& sessionId)
{
    QMutexLocker locker(&sessionMutex);
    
    auto it = activeSessions.find(sessionId);
    if (it != activeSessions.end()) {
        return it.value();
    }
    
    qCWarning(logDBusServer) << "Session not found:" << sessionId;
    return nullptr;
}

QList<SessionManager::SessionInfo> SessionManager::getAllSessions() const
{
    QMutexLocker locker(&sessionMutex);
    return sessions.values();
}

int SessionManager::getSessionCount() const
{
    QMutexLocker locker(&sessionMutex);
    return sessions.size();
}

void SessionManager::setTaskManager(TaskManager* taskManager)
{
    this->taskManager = taskManager;
    qCInfo(logDBusServer) << "TaskManager set for SessionManager";
}

TaskManager* SessionManager::getTaskManager() const
{
    return taskManager;
}

void SessionManager::setModelCenter(ModelCenter* modelCenter)
{
    this->modelCenter = modelCenter;
    qCInfo(logDBusServer) << "ModelCenter set for SessionManager";
}

ModelCenter* SessionManager::getModelCenter() const
{
    return modelCenter;
}

QString SessionManager::generateSessionId() const
{
    // Use Id128 format to generate a clean hex string without hyphens
    // This matches the original APIServer implementation
    return QUuid::createUuid().toString(QUuid::Id128);
}

QSharedPointer<SessionBase> SessionManager::createSessionInstance(AITypes::ServiceType type, const QString& sessionId)
{
    switch (type) {
        case AITypes::ServiceType::Chat:
            return QSharedPointer<TextChatSession>::create(sessionId, this);
        case AITypes::ServiceType::FunctionCalling:
            return QSharedPointer<FunctionCallingSession>::create(sessionId, this);
        case AITypes::ServiceType::SpeechToText:
            return QSharedPointer<SpeechToTextSession>::create(sessionId, this);
        case AITypes::ServiceType::TextToSpeech:
            return QSharedPointer<TextToSpeechSession>::create(sessionId, this);
        case AITypes::ServiceType::ImageRecognition:
            return QSharedPointer<ImageRecognitionSession>::create(sessionId, this);
        case AITypes::ServiceType::OCR:
            return QSharedPointer<OCRSession>::create(sessionId, this);
        default:
            qCWarning(logDBusServer) << "Unsupported service type:" << static_cast<int>(type);
            return nullptr;
    }
} 