// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORDB_SESSION_H
#define VECTORDB_SESSION_H

#include "aidaemon_global.h"

#include <QObject>
#include <QDateTime>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QVariantMap>
#include <QVariantList>
#include <QVariantHash>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class ProjectManager;
class ServiceConfig;
class VectorDb;

/**
 * @brief Session class for managing application-specific operations
 * 
 * Each session coordinates business operations for a specific application,
 * providing isolation and resource management.
 */
class Session : public QObject
{
    Q_OBJECT

public:
    explicit Session(const QString &appId);
    ~Session();

    /**
     * @brief Initialize the session and its components
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Clean up session resources
     */
    void cleanup();

    /**
     * @brief Get application ID
     * @return Application identifier
     */
    QString getAppId() const { return m_appId; }

    /**
     * @brief Get last access time
     * @return Last access timestamp
     */
    QDateTime getLastAccessTime() const { return m_lastAccessTime; }

    /**
     * @brief Check if session is active
     * @return true if active, false otherwise
     */
    bool isActive() const;

    /**
     * @brief Build vector index for files
     * @param docId of file to index
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @return Task ID for tracking progress
     */
    QString buildIndex(const QString &docId, const QVariantHash &extensionParams = QVariantHash());

    /**
     * @brief Delete vector index
     * @param all Index
     * @param extensionParams Optional extension parameters (model topk, etc.)
     * @return true on success, false on failure
     */
    bool destroyIndex(bool allIndex, const QVariantHash &extensionParams);

    /**
     * @brief Search for similar vectors
     * @param query Search query
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @return Search results
     */
    QVariantList search(const QString &query, const QVariantHash &extensionParams = QVariantHash());

    /**
     * @brief Search for similar vectors asynchronously
     * @param query Search query
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @return Task ID for tracking progress
     */
    QString searchAsync(const QString &query, const QVariantHash &extensionParams = QVariantHash());

    /**
     * @brief Upload files to index
     * @param files List of file paths to upload
     * @param extensionParams Optional extension parameters (model topk, etc.)
     * @return List of QVariantHash objects containing file operation results
     *         Each hash contains: file (original path), path (relative path after upload),
     *         documentID (file ID), status (operation status, 0 for success)
     */
    QList<QVariantHash> uploadDocuments(const QStringList &files, const QVariantHash &extensionParams);

    /**
     * @brief Delete documents from index
     * @param documentIds List of document IDs to delete
     * @return List of QVariantHash objects containing delete operation results
     *         Each hash contains: path (relative path after upload), documentID (file ID),
     *         status (operation status, 0 for success)
     */
    QList<QVariantHash> deleteDocuments(const QStringList &documentIds);

    /**
     * @brief Get index status
     * @return Index status information
     */
    QVariantMap getIndexStatus();

    /**
     * @brief Get index statistics
     * @param projectId Project identifier
     * @return Index statistics
     */
    QVariantMap getIndexStatistics(const QString &projectId);

    /**
     * @brief Get documents information
     * @param documentIds List of document IDs to query (empty means all documents)
     * @return List of document information
     */
    QVariantList documentsInfo(const QStringList &documentIds = QStringList());

    static QString getValidModel(const QString &model);
public Q_SLOTS:
    /**
     * @brief Update last access time
     */
    void updateLastAccessTime();

    /**
     * @brief Handle task completion
     * @param taskId Task identifier
     * @param success Whether task succeeded
     */
    void onTaskCompleted(const QString &taskId, bool success);

Q_SIGNALS:
    /**
     * @brief Emitted when session becomes idle
     */
    void sessionIdle();

    /**
     * @brief Emitted to report task progress
     * @param taskId Task identifier
     * @param percentage Progress percentage
     */
    void taskProgress(const QString &taskId, int percentage);

    /**
     * @brief Emitted when an error occurs
     * @param message Error message
     */
    void errorOccurred(const QString &message);

    /**
     * @brief Emitted when index build is completed
     * @param projectId Project identifier
     * @param taskId Task identifier
     * @param success Whether operation succeeded
     */
    void indexBuildCompleted(const QString &projectId, const QString &taskId, bool success);

    /**
     * @brief Emitted when search is completed
     * @param projectId Project identifier
     * @param results Search results
     */
    void searchCompleted(const QString &projectId, const QVariantList &results);

private:
    // Helper methods
    bool initializeComponents();
    void connectSignals();
    bool validateProjectId(const QString &projectId) const;
    bool validateTaskId(const QString &taskId) const;

    // Internal project management
    QString getOrCreateProjectId(const QString &modelId);
    QString createProject(const QString &modelId, const QVariantMap &config);
    QString findProjectByModelId(const QString &modelId);

    // VectorDb object management
    VectorDb* getOrCreateVectorDb(const QString &projectId);
    bool removeVectorDb(const QString &projectId);

    QPair<QString, QVariantMap> findProjectByDocument(const QString &docId);

    // No Use
    bool deleteProject(const QString &projectId);
    QVariantMap getProjectInfo(const QString &projectId);
    QVariantList getAllProjects();
    bool updateProjectConfig(const QString &projectId, const QVariantMap &config);

protected:
    QString m_appId;
    QDateTime m_lastAccessTime;
    bool m_isInitialized = false;

    // Core components
    QScopedPointer<ProjectManager> m_projectManager;
    ServiceConfig *m_serviceConfig;  // Raw pointer to singleton (managed externally)
    QMap<QString, QSharedPointer<VectorDb>> m_vectorDbs;  // projectId -> VectorDb mapping
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORDB_SESSION_H
