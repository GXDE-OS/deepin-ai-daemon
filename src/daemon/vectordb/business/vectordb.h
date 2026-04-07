// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORDB_H
#define VECTORDB_H

#include "aidaemon_global.h"

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QHash>
#include <QMutex>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class DataManager;
class VectorIndexManager;
class ServiceConfig;
class WorkflowOrchestrator;

/**
 * @brief Main business logic coordinator for vector database operations
 *
 * Handles core functionality of vector indexing, searching, and management
 * operations using stage-based task management and workflow orchestration.
 */
class VectorDb : public QObject
{
    Q_OBJECT

public:
    explicit VectorDb(const QString &workDir);
    ~VectorDb();

    /**
     * @brief Build vector index for files (asynchronous)
     * @param Doc ID List of file paths to index
     * @return Workflow ID for tracking progress
     */
    QString buildIndex(const QStringList &docIds);

    /**
     * @brief Upload files to index (synchronous)
     * @param files List of file paths to upload
     * @return List of QVariantHash objects containing file operation results
     *         Each hash contains: file (original path), path (relative path after upload),
     *         documentID (file ID), status (operation status, 0 for success)
     */
    QList<QVariantHash> uploadDocuments(const QStringList &files);

    /**
     * @brief Delete documents from index (synchronous)
     * @param documentIds List of document IDs to delete
     * @return List of QVariantHash objects containing delete operation results
     *         Each hash contains: path (relative path after upload), documentID (file ID),
     *         status (operation status, 0 for success)
     */
    QList<QVariantHash> deleteDocuments(const QStringList &documentIds);

    /**
     * @brief Delete entire index (synchronous)
     * @return true on success, false on failure
     */
    bool deleteIndex();

    /**
     * @brief Destroy index, including all documents and indexes, reset vectordb.
     * @return true on success, false on failure
     */
    bool destroyIndex();

    /**
     * @brief Search for similar vectors (synchronous)
     * @param query Search query text
     * @param topK Number of top results to return
     * @param options Search options
     * @return Search results as QVariantList
     */
    QVariantList search(const QString &query, int topK = 10, const QVariantMap &options = QVariantMap());

    /**
     * @brief Search for similar vectors (asynchronous)
     * @param query Search query text
     * @param topK Number of top results to return
     * @param options Search options
     * @return Workflow ID for tracking progress
     */
    QString searchAsync(const QString &query, int topK = 10, const QVariantMap &options = QVariantMap());

    /**
     * @brief Get index status information
     * @return Index status as QVariantMap
     */
    QVariantMap getIndexStatus() const;

    /**
     * @brief Get index statistics
     * @return Index statistics as QVariantMap
     */
    QVariantMap getIndexStatistics() const;

    /**
     * @brief Get workflow progress information
     * @param workflowId Workflow ID
     * @return Workflow progress as QVariantMap
     */
    QVariantMap getWorkflowProgress(const QString &workflowId) const;

    /**
     * @brief Get stage task progress information
     * @param taskId Task identifier
     * @return Task progress as QVariantMap
     */
    QVariantMap getStageTaskProgress(const QString &taskId) const;

    /**
     * @brief Cancel a running workflow
     * @param workflowId Workflow identifier
     * @return true on success, false on failure
     */
    bool cancelWorkflow(const QString &workflowId);

    /**
     * @brief Pause a running workflow
     * @param workflowId Workflow identifier
     * @return true on success, false on failure
     */
    bool pauseWorkflow(const QString &workflowId);

    /**
     * @brief Resume a paused workflow
     * @param workflowId Workflow identifier
     * @return true on success, false on failure
     */
    bool resumeWorkflow(const QString &workflowId);

    /**
     * @brief Get list of active workflows
     * @return List of active workflow IDs
     */
    QStringList getActiveWorkflows() const;

    /**
     * @brief Check if system is healthy
     * @return true if healthy, false otherwise
     */
    bool isHealthy() const;

    /**
     * @brief Get system status
     * @return System status information
     */
    QVariantMap getSystemStatus() const;

    /**
     * @brief Get work directory
     * @return Work directory path
     */
    QString getWorkDir() const { return m_workDir; }

    void setModelId(const QString &modelId);
    QString getModelId() const { return m_modelId; }

    /**
     * @brief Get all documents
     * @return List of all documents
     */
    QVariantList getAllDocuments() const;

    /**
     * @brief Get document information
     * @param documentId Document identifier
     * @return Document information
     */
    QVariantMap getDocumentInfo(const QString &documentId) const;

    bool isRunning();

public Q_SLOTS:
    /**
     * @brief Handle workflow completion
     * @param workflowId Workflow identifier
     * @param success Whether workflow succeeded
     */
    void onWorkflowCompleted(const QString &workflowId, bool success);

    /**
     * @brief Handle workflow progress update
     * @param workflowId Workflow identifier
     * @param completedStages Number of completed stages
     * @param totalStages Total number of stages
     */
    void onWorkflowProgress(const QString &workflowId, int completedStages, int totalStages);

    /**
     * @brief Handle workflow error
     * @param workflowId Workflow identifier
     * @param error Error message
     */
    void onWorkflowError(const QString &workflowId, const QString &error);



Q_SIGNALS:
    /**
     * @brief Emitted when index build workflow is completed
     * @param projectId Project identifier
     * @param workflowId Workflow identifier
     * @param success Whether operation succeeded
     * @param message Status message
     */
    void indexBuildWorkflowCompleted(const QString &projectId, const QString &workflowId, bool success, const QString &message);

    /**
     * @brief Emitted to report workflow progress
     * @param workflowId Workflow identifier
     * @param completedStages Number of completed stages
     * @param totalStages Total number of stages
     */
    void workflowProgress(const QString &workflowId, int completedStages, int totalStages);

    /**
     * @brief Emitted when search is completed
     * @param projectId Project identifier
     * @param results Search results
     */
    void searchCompleted(const QString &projectId, const QVariantList &results);

    /**
     * @brief Emitted when an error occurs
     * @param operation Operation that failed
     * @param error Error message
     */
    void errorOccurred(const QString &operation, const QString &error);

private:
    QString m_workDir;
    QString m_modelId;

    // Core components
    QScopedPointer<DataManager> m_dataManager;
    QScopedPointer<VectorIndexManager> m_vectorIndexManager;
    ServiceConfig *m_serviceConfig;  // Raw pointer to singleton (managed externally)

    // Workflow tracking
    QHash<QString, QSharedPointer<WorkflowOrchestrator>> m_activeWorkflows;
    QMutex m_workflowMutex;

    // Workflow management
    void registerWorkflow(QSharedPointer<WorkflowOrchestrator> workflow);
    void unregisterWorkflow(const QString &workflowId);
    
    // Workflow creation helpers
    QSharedPointer<WorkflowOrchestrator> createIndexBuildWorkflow(const QStringList &docIds);
    QSharedPointer<WorkflowOrchestrator> createDocumentUpdateWorkflow(const QStringList &files);
    QSharedPointer<WorkflowOrchestrator> createSearchWorkflow(const QString &query, int topK, const QVariantMap &options);
    QSharedPointer<WorkflowOrchestrator> createDeleteWorkflow(const QStringList &documentIds);

    // process workflow results
    bool processIndexBuildWorkflowRestults(const QVariantMap &results);
    bool processDataParsingResults(const QVariantMap &dataTask);
    QString processVectorIndexingResults(const QVariantMap &vectorTask);

    // Configuration and optimization
    void optimizeStageResourceAllocation();
    void updateStageThreadCounts();
    void cleanupCompletedWorkflows();
    void connectComponentSignals();
    void disconnectComponentSignals();
    // Helper methods

};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORDB_H
