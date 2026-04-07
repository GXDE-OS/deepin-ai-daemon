// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORDB_SESSIONMANAGER_H
#define VECTORDB_SESSIONMANAGER_H

#include "aidaemon_global.h"

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QMutex>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class Session;

/**
 * @brief Session manager for handling application sessions (Singleton)
 * 
 * Manages the lifecycle of sessions for different applications.
 * Each application gets its own isolated session for vector database operations.
 * Uses singleton pattern to ensure only one instance exists.
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance
     * @return Pointer to the singleton instance
     */
    static SessionManager* instance();
    
    /**
     * @brief Destroy the singleton instance
     */
    static void destroyInstance();
    
    ~SessionManager();

    /**
     * @brief Create a new session for an application
     * @param appId Application identifier
     * @return Pointer to the created session, nullptr on failure
     */
    Session* createSession(const QString &appId);

    /**
     * @brief Delete a session for an application
     * @param appId Application identifier
     */
    void deleteSession(const QString &appId);

    /**
     * @brief Get existing session for an application
     * @param appId Application identifier
     * @return Pointer to the session, nullptr if not found
     */
    Session* getSession(const QString &appId);

    /**
     * @brief Check if session exists for an application
     * @param appId Application identifier
     * @return true if session exists, false otherwise
     */
    bool hasSession(const QString &appId) const;

    /**
     * @brief Get number of active sessions
     * @return Number of active sessions
     */
    int getActiveSessionCount() const;

    /**
     * @brief Get list of active session app IDs
     * @return List of app IDs with active sessions
     */
    QStringList getActiveSessionIds() const;

    /**
     * @brief Set session timeout in milliseconds
     * @param timeoutMs Timeout in milliseconds
     */
    void setSessionTimeout(int timeoutMs);

    /**
     * @brief Get session timeout in milliseconds
     * @return Timeout in milliseconds
     */
    int getSessionTimeout() const;

public Q_SLOTS:
    /**
     * @brief Handle session timeout
     * @param appId Application identifier
     */
    void onSessionTimeout(const QString &appId);

    /**
     * @brief Update session access time
     * @param appId Application identifier
     */
    void updateSessionAccessTime(const QString &appId);

Q_SIGNALS:
    /**
     * @brief Emitted when a session is created
     * @param appId Application identifier
     */
    void sessionCreated(const QString &appId);

    /**
     * @brief Emitted when a session is deleted
     * @param appId Application identifier
     */
    void sessionDeleted(const QString &appId);

    /**
     * @brief Emitted when session count changes
     * @param count Current session count
     */
    void sessionCountChanged(int count);

private Q_SLOTS:
    void performCleanup();

private:
    explicit SessionManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(SessionManager)
    
    static SessionManager* s_instance;
    static QMutex s_instanceMutex;
    
    QHash<QString, QPointer<Session>> m_sessions;
    QTimer *m_cleanupTimer;
    mutable QMutex m_sessionMutex;
    
    static constexpr int SESSION_TIMEOUT_MS = 1800000; // 30 minutes
    static constexpr int MAX_SESSIONS = 50;
    static constexpr int CLEANUP_INTERVAL_MS = 300000; // 5 minutes
    
    int m_sessionTimeoutMs = SESSION_TIMEOUT_MS;

    bool validateAppId(const QString &appId) const;
    bool canCreateSession() const;
    void removeExpiredSessions();
    void initializeCleanupTimer();
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORDB_SESSIONMANAGER_H
