// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STAGEBASEDTASKMANAGER_H
#define STAGEBASEDTASKMANAGER_H

#include "aidaemon_global.h"

#include <QObject>
#include <QHash>
#include <QMap>
#include <QQueue>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QMutex>
#include <QTimer>
#include <QThreadPool>
#include <QVariantMap>
#include <QVariantList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class StageTask;
class WorkflowOrchestrator;

/**
 * @brief Stage-based task manager for optimized resource utilization
 * 
 * Manages multiple stage-specific task queues with independent scheduling
 * and resource management for better parallel execution and efficiency.
 */
class StageBasedTaskManager : public QObject
{
    Q_OBJECT

public:
    enum class ProcessingStage {
        DataParsing = 1,        // Data parsing and text extraction (CPU-intensive)
        EmbeddingGeneration = 2, // Text to vector conversion (GPU/Network-intensive)
        IndexBuilding = 3,      // Vector index construction (Memory-intensive)
        DataPersistence = 4,    // Database operations (I/O-intensive)
        Search = 5              // Vector search operations (CPU/Memory-intensive)
    };
    Q_ENUM(ProcessingStage)

    enum class StagePriority {
        Critical = 1,   // Search operations
        High = 2,       // Data persistence, cleanup
        Normal = 3,     // Regular embedding generation
        Low = 4         // Data parsing, index building
    };
    Q_ENUM(StagePriority)

    explicit StageBasedTaskManager();
    ~StageBasedTaskManager();

    /**
     * @brief Create data parsing task
     * @param documents List of documents to process
     * @param priority Task priority
     * @return Task ID
     */
    QString createDataParserTask(const QVariantMap &params, StagePriority priority = StagePriority::Low);

    /**
     * @brief Create embedding generation task
     * @param textChunks List of text chunks to embed
     * @param priority Task priority
     * @return Task ID
     */
    QString createEmbeddingTask(const QVariantMap &params, StagePriority priority = StagePriority::Normal);

    /**
     * @brief Create index building task
     * @param vectors List of vectors to index
     * @param priority Task priority
     * @return Task ID
     */
    QString createIndexBuildTask(const QVariantMap &params, StagePriority priority = StagePriority::Low);

    /**
     * @brief Create search task
     * @param query Search query
     * @param priority Task priority
     * @return Task ID
     */
    QString createSearchTask(const QVariantMap &params, StagePriority priority = StagePriority::Critical);

    /**
     * @brief Create data persistence task
     * @param data Data to persist
     * @param priority Task priority
     * @return Task ID
     */
    QString createPersistenceTask(const QVariantMap &data, StagePriority priority = StagePriority::High);

    /**
     * @brief Pause a workflow
     * @param workflowId Workflow identifier
     * @return true on success, false on failure
     */
    bool pauseWorkflow(const QString &workflowId);

    /**
     * @brief Resume a workflow
     * @param workflowId Workflow identifier
     * @return true on success, false on failure
     */
    bool resumeWorkflow(const QString &workflowId);

    /**
     * @brief Cancel a workflow
     * @param workflowId Workflow identifier
     * @return true on success, false on failure
     */
    bool cancelWorkflow(const QString &workflowId);

    /**
     * @brief Set thread count for a stage
     * @param stage Processing stage
     * @param threadCount Number of threads
     */
    void setStageThreadCount(ProcessingStage stage, int threadCount);

    /**
     * @brief Set resource limit for a stage
     * @param stage Processing stage
     * @param resourceType Resource type (memory, cpu, etc.)
     * @param limit Resource limit
     */
    void setStageResourceLimit(ProcessingStage stage, const QString &resourceType, qreal limit);

    /**
     * @brief Get stage queue length
     * @param stage Processing stage
     * @return Number of pending tasks in queue
     */
    int getStageQueueLength(ProcessingStage stage) const;

    /**
     * @brief Get stage statistics
     * @param stage Processing stage
     * @return Stage statistics as QVariantMap
     */
    QVariantMap getStageStatistics(ProcessingStage stage) const;

    /**
     * @brief Get task information
     * @param taskId Task identifier
     * @return Task information as QVariantMap
     */
    QVariantMap getTaskInfo(const QString &taskId) const;

    /**
     * @brief Get workflow information
     * @param workflowId Workflow identifier
     * @return Workflow information as QVariantMap
     */
    QVariantMap getWorkflowInfo(const QString &workflowId) const;

    /**
     * @brief Get active tasks by stage
     * @param stage Processing stage
     * @return List of active task IDs
     */
    QVariantList getActiveTasksByStage(ProcessingStage stage) const;

    /**
     * @brief Cancel a task
     * @param taskId Task identifier
     * @return true on success, false on failure
     */
    bool cancelTask(const QString &taskId);

    /**
     * @brief Enable/disable random execution for a stage
     * @param stage Processing stage
     * @param enabled Whether to enable random execution
     */
    void enableRandomExecution(ProcessingStage stage, bool enabled);

    /**
     * @brief Set scheduling strategy for a stage
     * @param stage Processing stage
     * @param strategy Scheduling strategy name
     */
    void setStageSchedulingStrategy(ProcessingStage stage, const QString &strategy);

public Q_SLOTS:
    /**
     * @brief Handle stage task completion
     * @param taskId Task identifier
     * @param results Task results
     */
    void onStageTaskCompleted(const QString &taskId, const QVariantMap &results);

    /**
     * @brief Handle stage task error
     * @param taskId Task identifier
     * @param error Error message
     */
    void onStageTaskError(const QString &taskId, const QString &error);

    /**
     * @brief Handle resource usage change
     * @param stage Processing stage
     * @param resourceType Resource type
     * @param usage Current usage
     */
    void onResourceUsageChanged(ProcessingStage stage, const QString &resourceType, qreal usage);

    /**
     * @brief Process stage queues
     */
    void processStageQueues();

Q_SIGNALS:
    /**
     * @brief Emitted when a stage task starts
     * @param taskId Task identifier
     * @param stage Processing stage
     */
    void stageTaskStarted(const QString &taskId, ProcessingStage stage);

    /**
     * @brief Emitted when a stage task completes
     * @param taskId Task identifier
     * @param stage Processing stage
     * @param success Whether task succeeded
     * @param results Task results (empty if failed)
     */
    void stageTaskCompleted(const QString &taskId, ProcessingStage stage, bool success, const QVariantMap &results = QVariantMap());

    /**
     * @brief Emitted to report task progress
     * @param taskId Task identifier
     * @param percentage Progress percentage
     */
    void stageTaskProgress(const QString &taskId, int percentage);

    /**
     * @brief Emitted when a workflow completes
     * @param workflowId Workflow identifier
     * @param success Whether workflow succeeded
     */
    void workflowCompleted(const QString &workflowId, bool success);

    /**
     * @brief Emitted to report workflow progress
     * @param workflowId Workflow identifier
     * @param completedStages Number of completed stages
     * @param totalStages Total number of stages
     */
    void workflowProgress(const QString &workflowId, int completedStages, int totalStages);

    /**
     * @brief Emitted when stage queue status changes
     * @param stage Processing stage
     * @param pendingTasks Number of pending tasks
     * @param runningTasks Number of running tasks
     */
    void stageQueueStatusChanged(ProcessingStage stage, int pendingTasks, int runningTasks);

private:
    // Stage-specific task queues with priority sub-queues
    struct StageQueue {
        QQueue<QString> criticalQueue;
        QQueue<QString> highQueue;
        QQueue<QString> normalQueue;
        QQueue<QString> lowQueue;
        QScopedPointer<QThreadPool> threadPool;
        QMutex queueMutex;
        bool randomExecutionEnabled = true;
        QString schedulingStrategy = "priority_with_random";
    };
    
    QMap<ProcessingStage, QSharedPointer<StageQueue>> m_stageQueues;
    
    // Task and workflow tracking
    QHash<QString, QSharedPointer<StageTask>> m_stageTasks;
    QHash<QString, QSharedPointer<WorkflowOrchestrator>> m_workflows;
    QMap<ProcessingStage, QSet<QString>> m_runningTasksByStage;
    
    // Resource monitoring per stage
    QMap<ProcessingStage, QVariantMap> m_stageResourceLimits;
    QMap<ProcessingStage, QVariantMap> m_stageResourceUsage;
    
    // Scheduling and coordination
    QTimer *m_globalSchedulerTimer;
    QMutex m_globalMutex;
    
    // Helper methods
    QString generateTaskId() const;
    QString generateWorkflowId() const;
    QQueue<QString>& getStageQueue(ProcessingStage stage, StagePriority priority);
    void scheduleNextStageTask(ProcessingStage stage);
    bool canStartStageTask(ProcessingStage stage, const QSharedPointer<StageTask> &task) const;
    QString selectNextTaskFromQueue(ProcessingStage stage);
    void updateStageResourceUsage(ProcessingStage stage);
    void cleanupCompletedTasks();
    
    // Workflow coordination
    void processWorkflowSteps();
    void advanceWorkflow(const QString &workflowId, const QString &completedTaskId);
    
    // Initialization
    void initializeStageQueues();
    void initializeScheduler();
    
    // Task creation
    QSharedPointer<StageTask> createStageTask(ProcessingStage stage, const QString &taskId, const QVariantMap &params);
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // STAGEBASEDTASKMANAGER_H
