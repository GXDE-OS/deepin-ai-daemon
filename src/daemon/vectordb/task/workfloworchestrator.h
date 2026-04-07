// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WORKFLOWORCHESTRATOR_H
#define WORKFLOWORCHESTRATOR_H

#include "aidaemon_global.h"
#include "stagebasedtaskmanager.h"

#include <QObject>
#include <QVector>
#include <QSet>
#include <QDateTime>
#include <QVariantMap>
#include <QScopedPointer>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class StageBasedTaskManager;

/**
 * @brief Workflow orchestrator for coordinating multi-stage workflows
 * 
 * Manages dependencies between stage tasks and coordinates their execution
 * to implement complex multi-stage workflows like document indexing.
 */
class WorkflowOrchestrator : public QObject
{
    Q_OBJECT

public:
    enum class WorkflowType {
        IndexBuilding = 1,      // Data Parsing -> Embedding -> Index Building (3 tasks)
        DocumentUpdate = 2,     // Data Parsing -> Embedding -> Index Update (3 tasks)
        Search = 3,             // Query Embedding (1 task)
        Delete = 4,             // Direct deletion (0 tasks)
        Maintenance = 5         // Index Optimization -> Cleanup -> Validation
    };
    Q_ENUM(WorkflowType)

    enum class WorkflowStatus {
        Pending = 1,
        Running = 2,
        Paused = 3,
        Completed = 4,
        Failed = 5,
        Cancelled = 6
    };
    Q_ENUM(WorkflowStatus)

    explicit WorkflowOrchestrator(WorkflowType type);
    ~WorkflowOrchestrator();

    /**
     * @brief Create and setup workflow of specified type
     * @param workflowType Type of workflow to create
     * @param data Data to process (documents, queries, etc.)
     * @return Configured workflow ready to start
     */
    static QSharedPointer<WorkflowOrchestrator> createWorkflow(WorkflowType workflowType, const QVariantMap &params);

    /**
     * @brief Start workflow execution
     * @return true on success, false on failure
     */
    bool startWorkflow();

    /**
     * @brief Pause workflow execution
     * @return true on success, false on failure
     */
    bool pauseWorkflow();

    /**
     * @brief Resume workflow execution
     * @return true on success, false on failure
     */
    bool resumeWorkflow();

    /**
     * @brief Cancel workflow execution
     * @return true on success, false on failure
     */
    bool cancelWorkflow();

    // Getters
    QString getWorkflowId() const { return m_workflowId; }
    WorkflowType getWorkflowType() const { return m_workflowType; }
    WorkflowStatus getStatus() const { return m_status; }
    int getCompletedStages() const { return m_completedStages; }
    int getTotalStages() const { return m_totalStages; }
    qreal getOverallProgress() const;
    QVariantMap getWorkflowResults() const {return m_workflowResults; }

    /**
     * @brief Add a stage step to the workflow
     * @param stage Processing stage
     * @param taskParams Task parameters
     * @param priority Task priority
     */
    void addStageStep(StageBasedTaskManager::ProcessingStage stage, const QVariantMap &taskParams, 
                      StageBasedTaskManager::StagePriority priority = StageBasedTaskManager::StagePriority::Normal);

    /**
     * @brief Add dependency between stages
     * @param stageIndex Stage index that depends
     * @param dependsOnStageIndex Stage index it depends on
     */
    void addDependency(int stageIndex, int dependsOnStageIndex);

    /**
     * @brief Check if stage is ready to execute
     * @param stageIndex Stage index
     * @return true if ready, false otherwise
     */
    bool isStageReady(int stageIndex) const;

    /**
     * @brief Save workflow state to checkpoint
     * @return true on success, false on failure
     */
    bool saveWorkflowState() const;

    /**
     * @brief Load workflow state from checkpoint
     * @return true on success, false on failure
     */
    bool loadWorkflowState();

    /**
     * @brief Get workflow checkpoint data
     * @return Checkpoint data as QVariantMap
     */
    QVariantMap getWorkflowCheckpoint() const;

public Q_SLOTS:
    /**
     * @brief Handle stage task completion
     * @param taskId Task identifier
     * @param stage Processing stage
     * @param success Whether task succeeded
     * @param results Task results (empty if failed)
     */
    void onStageTaskCompleted(const QString &taskId, StageBasedTaskManager::ProcessingStage stage, bool success, const QVariantMap &results = QVariantMap());

    /**
     * @brief Handle stage task progress
     * @param taskId Task identifier
     * @param percentage Progress percentage
     */
    void onStageTaskProgress(const QString &taskId, int percentage);

Q_SIGNALS:
    /**
     * @brief Emitted when workflow starts
     * @param workflowId Workflow identifier
     */
    void workflowStarted(const QString &workflowId);

    /**
     * @brief Emitted when workflow completes
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
     * @brief Emitted when a workflow stage starts
     * @param workflowId Workflow identifier
     * @param stageIndex Stage index
     * @param stage Processing stage
     */
    void workflowStageStarted(const QString &workflowId, int stageIndex, StageBasedTaskManager::ProcessingStage stage);

    /**
     * @brief Emitted when a workflow stage completes
     * @param workflowId Workflow identifier
     * @param stageIndex Stage index
     * @param success Whether stage succeeded
     */
    void workflowStageCompleted(const QString &workflowId, int stageIndex, bool success);

    /**
     * @brief Emitted when workflow encounters error
     * @param workflowId Workflow identifier
     * @param error Error message
     */
    void workflowError(const QString &workflowId, const QString &error);

private:
    struct WorkflowStage {
        StageBasedTaskManager::ProcessingStage stage;
        QVariantMap taskParams;
        StageBasedTaskManager::StagePriority priority;
        QString taskId;
        bool isCompleted = false;
        bool hasFailed = false;
        QSet<int> dependencies;
        QVariantMap results;
    };

    QString m_workflowId;
    WorkflowType m_workflowType;
    WorkflowStatus m_status = WorkflowStatus::Pending;
    QScopedPointer<StageBasedTaskManager> m_taskManager;

    QVector<WorkflowStage> m_stages;
    int m_completedStages = 0;
    int m_totalStages = 0;
    int m_currentStageIndex = -1;

    // Workflow state
    QDateTime m_startTime;
    QDateTime m_endTime;
    QVariantMap m_workflowResults;
    QString m_checkpointFilePath;

    // Helper methods
    void processNextStage();
    bool canStartStage(int stageIndex) const;
    void startStage(int stageIndex);
    void completeStage(int stageIndex, const QVariantMap &results);
    void failStage(int stageIndex, const QString &error);
    void updateWorkflowProgress();
    QVariantMap collectWorkflowResults() const;
    void initializeCheckpointFile();
    void updateCheckpointFile() const;
    void setStatus(WorkflowStatus status);
    
    // Result validation methods
    QVariantMap validateStageResults(StageBasedTaskManager::ProcessingStage stage, const QVariantMap &results) const;
    bool isResultsOptional(StageBasedTaskManager::ProcessingStage stage) const;
    QVariantMap getStageResultsFromPrevious(int stageIndex, const QString &sourceStage) const;
    
    // Predefined workflow templates
    void setupIndexBuildingWorkflow(const QVariantMap &params);
    void setupDocumentUpdateWorkflow(const QVariantMap &params);
    void setupSearchWorkflow(const QVariantMap &params);
    void setupDeleteWorkflow(const QVariantMap &params);
    void setupMaintenanceWorkflow(const QVariantMap &params);
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // WORKFLOWORCHESTRATOR_H
