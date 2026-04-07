// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef STAGETASK_H
#define STAGETASK_H

#include "aidaemon_global.h"
#include "stagebasedtaskmanager.h"

#include <QObject>
#include <QRunnable>
#include <QDateTime>
#include <QVariantMap>
#include <QWaitCondition>
#include <QMutex>

#include <atomic>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class StageTask;

/**
 * @brief Task runner for executing stage tasks in thread pool
 * 
 * Handles the QRunnable interface and delegates execution to StageTask.
 */
class TaskRunner : public QRunnable
{
public:
    explicit TaskRunner(StageTask *task);
    ~TaskRunner() override;

    // QRunnable interface
    void run() override;

private:
    StageTask *m_task;
};

/**
 * @brief Abstract base class for stage-specific tasks
 * 
 * Provides resource-aware execution framework for stage-specific tasks
 * with support for pause/resume, checkpointing, and progress reporting.
 */
class StageTask : public QObject
{
    Q_OBJECT

public:
    enum class TaskStatus {
        Pending = 1,
        Running = 2,
        Paused = 3,
        Completed = 4,
        Failed = 5,
        Cancelled = 6
    };
    Q_ENUM(TaskStatus)

    explicit StageTask(const QString &taskId, StageBasedTaskManager::ProcessingStage stage);
    virtual ~StageTask();

    /**
     * @brief Create a TaskRunner for this task
     * @return TaskRunner instance for thread pool execution
     */
    TaskRunner* createRunner();

    /**
     * @brief Execute the task (called by TaskRunner)
     */
    void executeTask();

    /**
     * @brief Pause task execution
     */
    virtual void pause();

    /**
     * @brief Resume task execution
     */
    virtual void resume();

    /**
     * @brief Cancel task execution
     */
    virtual void cancel();

    /**
     * @brief Save task checkpoint
     * @return true on success, false on failure
     */
    virtual bool saveCheckpoint() const;

    /**
     * @brief Load task checkpoint
     * @param checkpoint Checkpoint data
     * @return true on success, false on failure
     */
    virtual bool loadCheckpoint(const QVariantMap &checkpoint);

    // Getters
    QString getTaskId() const { return m_taskId; }
    QString getProjectId() const { return m_projectInfo.value("id").toString(); }
    QVariantMap getProjectInfo() const { return m_projectInfo; }
    StageBasedTaskManager::ProcessingStage getStage() const { return m_stage; }
    TaskStatus getStatus() const { return m_status; }
    int getProgress() const { return m_progress; }
    QDateTime getStartTime() const { return m_startTime; }
    QDateTime getEndTime() const { return m_endTime; }

    /**
     * @brief Get resource requirements for scheduling
     * @return Resource requirements as QVariantMap
     */
    virtual QVariantMap getResourceRequirements() const = 0;

    /**
     * @brief Get estimated duration in seconds
     * @return Estimated duration
     */
    virtual qreal getEstimatedDuration() const = 0;

    /**
     * @brief Get primary resource type
     * @return Resource type (cpu, memory, gpu, network, io)
     */
    virtual QString getResourceType() const = 0;

public Q_SLOTS:
    /**
     * @brief Update task progress
     * @param percentage Progress percentage (0-100)
     */
    void updateProgress(int percentage);

    /**
     * @brief Update task status
     * @param status New task status
     */
    void updateStatus(TaskStatus status);

Q_SIGNALS:
    /**
     * @brief Emitted when progress changes
     * @param taskId Task identifier
     * @param percentage Progress percentage
     */
    void progressChanged(const QString &taskId, int percentage);

    /**
     * @brief Emitted when status changes
     * @param taskId Task identifier
     * @param status New status
     */
    void statusChanged(const QString &taskId, TaskStatus status);

    /**
     * @brief Emitted when task completes
     * @param taskId Task identifier
     * @param results Task results
     */
    void taskCompleted(const QString &taskId, const QVariantMap &results);

    /**
     * @brief Emitted when task encounters error
     * @param taskId Task identifier
     * @param error Error message
     */
    void taskError(const QString &taskId, const QString &error);

    /**
     * @brief Emitted when checkpoint is ready
     * @param taskId Task identifier
     * @param checkpoint Checkpoint data
     */
    void checkpointReady(const QString &taskId, const QVariantMap &checkpoint);

protected:
    /**
     * @brief Initialize task execution (implemented by subclasses)
     * @return true on success, false on failure
     */
    virtual bool initializeExecution() = 0;

    /**
     * @brief Execute the main task logic (implemented by subclasses)
     * @return Task results
     */
    virtual QVariantMap executeStage() = 0;

    /**
     * @brief Finalize task execution (implemented by subclasses)
     */
    virtual void finalizeExecution() = 0;

    /**
     * @brief Clean up task resources (implemented by subclasses)
     */
    virtual void cleanupResources() {}

    // Helper methods for subclasses
    void setProgress(int percentage);
    void setStatus(TaskStatus status);
    bool checkCancellation() const;
    bool checkPause();
    void saveProgressCheckpoint();

    // Checkpoint management
    virtual void saveExecutionData(const QVariantMap &data);
    virtual QVariantMap loadExecutionData() const;

private:
    QString m_taskId;
    QVariantMap m_projectInfo;
    StageBasedTaskManager::ProcessingStage m_stage;
    TaskStatus m_status = TaskStatus::Pending;
    int m_progress = 0;
    QDateTime m_startTime;
    QDateTime m_endTime;

    // Execution control
    std::atomic<bool> m_isPaused{false};
    std::atomic<bool> m_isCancelled{false};
    QWaitCondition m_pauseCondition;
    QMutex m_pauseMutex;

    // Checkpoint data
    mutable QMutex m_checkpointMutex;
    QVariantMap m_checkpointData;
    QString m_checkpointFilePath;

    // Execution methods
    void executeInternal();
    bool waitForResume();
    void updateCheckpointFile() const;
    bool loadCheckpointFile();
    void initializeCheckpointFile();
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // STAGETASK_H
