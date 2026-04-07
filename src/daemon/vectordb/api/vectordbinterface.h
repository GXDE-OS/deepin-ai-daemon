// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORDBINTERFACE_H
#define VECTORDBINTERFACE_H

#include "aidaemon_global.h"
#include "vectordb_define.h"
#include "vectordbreply.h"

#include <QObject>
#include <QDBusMessage>
#include <QStringList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class SessionManager;
class Session;
class ModelRepository;

/**
 * @brief Unified D-Bus interface for all vector database operations
 * 
 * This class provides a single D-Bus interface that combines:
 * - Service configuration operations  
 * - Vector database operations
 * - Session management (internal)
 */
class VectorDbInterface : public QObject
{
    Q_OBJECT

public:
    explicit VectorDbInterface(QObject *parent = nullptr);
    ~VectorDbInterface();

public Q_SLOTS:
    // ========== Service Configuration Operations ==========

    /**
     * @brief Set global configuration value
     * @param key Configuration key
     * @param value Configuration value
     * @return true on success, false on failure
     */
    bool setGlobalConfig(const QString &key, const QVariant &value);

    /**
     * @brief Get global configuration value
     * @param key Configuration key
     * @return Configuration value
     */
    QVariant getGlobalConfig(const QString &key);

    /**
     * @brief Get all global configurations
     * @return JSON string of all configurations
     */
    QString getAllConfigs();

    /**
     * @brief Reset configurations to defaults
     * @return true on success, false on failure
     */
    bool resetToDefaults();

    /**
     * @brief Set resource limit
     * @param resource Resource type (memory, cpu, disk, etc.)
     * @param limit Limit value
     * @return true on success, false on failure
     */
    bool setResourceLimit(const QString &resource, int limit);

    /**
     * @brief Get resource limit
     * @param resource Resource type
     * @return Current limit value
     */
    int getResourceLimit(const QString &resource);

    /**
     * @brief Get system status
     * @return JSON string of system status information
     */
    QString getSystemStatus();

    // ========== Vector Database Operations ==========

    /**
     * @brief Build vector index for files (asynchronous)
     * @param appId Application identifier
     * @param files List of file paths to index
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @param reply message for delayed reply
     * @return JSON string containing task ID
     */
    QString buildIndex(const QString &appId, const QString &docId, const QString &extensionParams, VectorDbReplyPtr reply);

    /**
     * @brief Upload files to Knowledgebase
     * @param appId Application identifier
     * @param files List of file paths to upload
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @param reply message for delayed reply
     * @return JSON string containing array of documents.
     */
    QString uploadDocuments(const QString &appId, const QStringList &files, const QString &extensionParams, VectorDbReplyPtr reply);

    /**
     * @brief Delete documents from index
     * @param appId Application identifier
     * @param documentIds List of document IDs to delete
     * @param reply message for delayed reply
     * @return JSON string containing delete operation results
     */
    QString deleteDocuments(const QString &appId, const QStringList &documentIds, VectorDbReplyPtr reply);

    /**
     * @brief Delete all index
     * @param appId Application identifier
     * @param all Index
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @param reply message for delayed reply
     * @return true on success, false on failure
     */
    QString destroyIndex(const QString &appId, bool allIndex, const QString &extensionParams, VectorDbReplyPtr reply);

    /**
     * @brief Search documents by query
     * @param appId Application identifier
     * @param query Search query
     * @param extensionParams Optional extension parameters as JSON string (model, topk, etc.)
     * @param reply message for delayed reply
     * @return JSON string of search results
     */
    QString search(const QString &appId, const QString &query, const QString &extensionParams, VectorDbReplyPtr reply);

    /**
     * @brief Search documents asynchronously
     * @param appId Application identifier
     * @param query Search query
     * @param extensionParams Optional extension parameters (model, topk, etc.)
     * @return Task ID for tracking progress
     */
    QString searchAsync(const QString &appId, const QString &query, const QString &extensionParams = QString());

    /**
     * @brief Get index status and information
     * @param appId Application identifier
     * @return JSON string of index status information
     */
    QString getIndexStatus(const QString &appId);

    /**
     * @brief Get task progress information
     * @param taskId Task identifier
     * @return JSON string of task progress information
     */
    QString getTaskProgress(const QString &taskId);

    /**
     * @brief Cancel a running task
     * @param taskId Task identifier
     * @param reply message for delayed reply
     * @return true on success, false on failure
     */
    bool cancelTask(const QString &taskId, VectorDbReplyPtr reply);

    /**
     * @brief Get workflow progress information
     * @param workflowId Workflow identifier
     * @return JSON string of workflow progress information
     */
    QString getWorkflowProgress(const QString &workflowId);

    /**
     * @brief Get documents information
     * @param appId Application identifier
     * @param documentIds List of document IDs to query (empty means all documents)
     * @param reply message for delayed reply
     * @return JSON string of document information
     */
    QString documentsInfo(const QString &appId, const QStringList &documentIds, VectorDbReplyPtr reply);

    /**
     * @brief Get available embedding models
     * @param reply message for delayed reply
     * @return JSON string containing list of available embedding models
     */
    QString embeddingModels(VectorDbReplyPtr reply);

    QString embeddingTexts(const QString &appId, const QStringList &texts, const QString &extensionParams, VectorDbReplyPtr reply);

Q_SIGNALS:
    // Vector database operation signals
    void indexBuildStarted(const QString &appId, const QString &taskId);
    void indexBuildCompleted(const QString &appId, const QString &taskId, const QString &message);
    void indexBuildProgress(const QString &appId, const QString &taskId, int progress);
    
    void documentsAdded(const QString &appId, const QStringList &sourcefiles, const QStringList &docIDs);
    void documentsDeleted(const QString &appId, const QStringList &docIDs, const QStringList &filePaths);
    
    void searchCompleted(const QString &appId, const QString &results);
    void searchStarted(const QString &appId, const QString &taskId);
    
    void taskStarted(const QString &taskId, const QString &operation);
    void taskCompleted(const QString &taskId, bool success, const QString &message);
    void taskProgress(const QString &taskId, int progress, const QString &stage);
    void taskCancelled(const QString &taskId);
    
    void errorOccurred(const QString &operation, const QString &error);

private:
    SessionManager *m_sessionManager;

    // Helper methods
    Session* getOrCreateSession(const QString &appId);
    bool validateAppId(const QString &appId) const;
    bool validateTaskId(const QString &taskId) const;
    bool validateConfig(const QString &config) const;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORDBINTERFACE_H
