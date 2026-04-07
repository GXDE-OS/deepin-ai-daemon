// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sessionmanager.h"
#include "session.h"

#include <QLoggingCategory>
#include <QMutexLocker>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(sessionManager, "vectordb.session.manager")

SessionManager* SessionManager::s_instance = nullptr;
QMutex SessionManager::s_instanceMutex;

SessionManager* SessionManager::instance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (!s_instance) {
        s_instance = new SessionManager();
    }
    return s_instance;
}

void SessionManager::destroyInstance()
{
    QMutexLocker locker(&s_instanceMutex);
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
{
    qCDebug(sessionManager) << "SessionManager initialized";
    
    //initializeCleanupTimer(); TODO: 闲置超过限制时间清理session资源
}

SessionManager::~SessionManager()
{
    qCDebug(sessionManager) << "SessionManager destroyed";
    
    // Clean up all sessions
    QMutexLocker locker(&m_sessionMutex);
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (it.value()) {
            it.value()->deleteLater();
        }
    }
    m_sessions.clear();
}

Session* SessionManager::createSession(const QString &appId)
{
    qCDebug(sessionManager) << "Creating session for appId:" << appId;
    
    if (!validateAppId(appId)) {
        qCWarning(sessionManager) << "Invalid appId:" << appId;
        return nullptr;
    }
    
    Session *existingSession = nullptr;
    bool canCreate = false;
    
    {
        QMutexLocker locker(&m_sessionMutex);
        
        // Check if session already exists
        if (m_sessions.contains(appId) && m_sessions[appId]) {
            qCDebug(sessionManager) << "Session already exists for appId:" << appId;
            existingSession = m_sessions[appId];
        } else {
            // Check session limits
            canCreate = canCreateSession();
        }
    }
    
    if (existingSession) {
        return existingSession;
    }
    
    if (!canCreate) {
        qCWarning(sessionManager) << "Cannot create session, limit reached";
        return nullptr;
    }
    
    // Create new session outside of lock
    Session *session = new Session(appId);
    if (!session->initialize()) {
        qCWarning(sessionManager) << "Failed to initialize session for appId:" << appId;
        session->deleteLater();
        return nullptr;
    }
    
    // Connect session signals outside of lock
    connect(session, &Session::sessionIdle, this, [this, appId]() {
        onSessionTimeout(appId);
    }, Qt::QueuedConnection);
    
    int sessionCount = 0;
    {
        QMutexLocker locker(&m_sessionMutex);
        
        // Double-check pattern: verify session doesn't exist before adding
        if (m_sessions.contains(appId) && m_sessions[appId]) {
            // Another thread created the session while we were outside the lock
            session->deleteLater();
            return m_sessions[appId];
        }
        
        m_sessions[appId] = session;
        sessionCount = m_sessions.size();
    }
    
    // Emit signals outside of lock
    emit sessionCreated(appId);
    emit sessionCountChanged(sessionCount);
    
    qCDebug(sessionManager) << "Session created successfully for appId:" << appId;
    return session;
}

void SessionManager::deleteSession(const QString &appId)
{
    qCDebug(sessionManager) << "Deleting session for appId:" << appId;
    
    Session *sessionToDelete = nullptr;
    int sessionCount = 0;
    bool sessionFound = false;
    
    {
        QMutexLocker locker(&m_sessionMutex);
        
        auto it = m_sessions.find(appId);
        if (it != m_sessions.end()) {
            sessionToDelete = it.value();
            m_sessions.erase(it);
            sessionCount = m_sessions.size();
            sessionFound = true;
        }
    }
    
    if (sessionFound) {
        if (sessionToDelete) {
            sessionToDelete->cleanup();
            sessionToDelete->deleteLater();
        }
        
        emit sessionDeleted(appId);
        emit sessionCountChanged(sessionCount);
        
        qCDebug(sessionManager) << "Session deleted for appId:" << appId;
    }
}

Session* SessionManager::getSession(const QString &appId)
{
    Session *session = nullptr;
    
    {
        QMutexLocker locker(&m_sessionMutex);
        
        auto it = m_sessions.find(appId);
        if (it != m_sessions.end() && it.value()) {
            session = it.value();
        }
    }
    
    if (session) {
        session->updateLastAccessTime();
    }
    
    return session;
}

bool SessionManager::hasSession(const QString &appId) const
{
    QMutexLocker locker(&m_sessionMutex);
    return m_sessions.contains(appId) && m_sessions[appId];
}

int SessionManager::getActiveSessionCount() const
{
    QMutexLocker locker(&m_sessionMutex);
    return m_sessions.size();
}

QStringList SessionManager::getActiveSessionIds() const
{
    QMutexLocker locker(&m_sessionMutex);
    return m_sessions.keys();
}

void SessionManager::setSessionTimeout(int timeoutMs)
{
    m_sessionTimeoutMs = timeoutMs;
    qCDebug(sessionManager) << "Session timeout set to:" << timeoutMs << "ms";
}

int SessionManager::getSessionTimeout() const
{
    return m_sessionTimeoutMs;
}

void SessionManager::onSessionTimeout(const QString &appId)
{
    qCDebug(sessionManager) << "Session timeout for appId:" << appId;
    deleteSession(appId);
}

void SessionManager::updateSessionAccessTime(const QString &appId)
{
    Session *session = getSession(appId);
    if (session) {
        session->updateLastAccessTime();
    }
}

void SessionManager::performCleanup()
{
    removeExpiredSessions();
}

bool SessionManager::validateAppId(const QString &appId) const
{
    return !appId.isEmpty() && appId.length() <= 128;
}

bool SessionManager::canCreateSession() const
{
    return m_sessions.size() < MAX_SESSIONS;
}

void SessionManager::removeExpiredSessions()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QStringList expiredSessions;
    
    {
        QMutexLocker locker(&m_sessionMutex);
        
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (!it.value()) {
                expiredSessions.append(it.key());
                continue;
            }
            
            QDateTime lastAccess = it.value()->getLastAccessTime();
            if (lastAccess.msecsTo(currentTime) > m_sessionTimeoutMs && !it.value()->isActive()) {
                expiredSessions.append(it.key());
            }
        }
    }
    
    // Remove expired sessions using deleteSession for consistency
    for (const QString &appId : expiredSessions) {
        deleteSession(appId);
        qCDebug(sessionManager) << "Expired session removed for appId:" << appId;
    }
}

void SessionManager::initializeCleanupTimer()
{
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL_MS);
    connect(m_cleanupTimer, &QTimer::timeout, this, &SessionManager::performCleanup);
    m_cleanupTimer->start();
    
    qCDebug(sessionManager) << "Cleanup timer initialized with interval:" << CLEANUP_INTERVAL_MS << "ms";
}
