// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stagetask.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QMutexLocker>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(stageTask, "vectordb.task.stage")

// TaskRunner implementation
TaskRunner::TaskRunner(StageTask *task)
    : m_task(task)
{
    setAutoDelete(true); // We manage deletion ourselves
}

TaskRunner::~TaskRunner()
{
}

void TaskRunner::run()
{
    if (m_task) {
        m_task->executeTask();
    }
}

StageTask::StageTask(const QString &taskId, StageBasedTaskManager::ProcessingStage stage)
    : QObject(nullptr)
    , m_taskId(taskId)
    , m_stage(stage)
{
    qCDebug(stageTask) << "StageTask created:" << taskId << "stage:" << static_cast<int>(stage);
    
    initializeCheckpointFile();
}

StageTask::~StageTask()
{
    qCDebug(stageTask) << "StageTask destroyed:" << m_taskId;
}

TaskRunner* StageTask::createRunner()
{
    return new TaskRunner(this);
}

void StageTask::executeTask()
{
    qCDebug(stageTask) << "Starting task execution:" << m_taskId;
    
    m_startTime = QDateTime::currentDateTime();
    setStatus(TaskStatus::Running);
    
    executeInternal();
}

void StageTask::pause()
{
    qCDebug(stageTask) << "Pausing task:" << m_taskId;
    
    m_isPaused = true;
    setStatus(TaskStatus::Paused);
}

void StageTask::resume()
{
    qCDebug(stageTask) << "Resuming task:" << m_taskId;
    
    QMutexLocker locker(&m_pauseMutex);
    m_isPaused = false;
    m_pauseCondition.wakeAll();
    setStatus(TaskStatus::Running);
}

void StageTask::cancel()
{
    qCDebug(stageTask) << "Cancelling task:" << m_taskId;
    
    m_isCancelled = true;
    
    // Wake up if paused
    QMutexLocker locker(&m_pauseMutex);
    m_pauseCondition.wakeAll();
    
    setStatus(TaskStatus::Cancelled);
}

bool StageTask::saveCheckpoint() const
{
    QMutexLocker locker(&m_checkpointMutex);
    if (m_checkpointFilePath.isEmpty()) {
        return false;
    }

    QFile file(m_checkpointFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc = QJsonDocument::fromVariant(m_checkpointData);
        file.write(doc.toJson());
        file.close();
    } else {
        qCWarning(stageTask) << "Failed to save checkpoint file:" << m_checkpointFilePath;
        return false;
    }

    return true;
}

bool StageTask::loadCheckpoint(const QVariantMap &checkpoint)
{
    QMutexLocker locker(&m_checkpointMutex);
    
    m_checkpointData = checkpoint;
    
    // Restore basic state
    m_progress = checkpoint.value("progress", 0).toInt();
    m_startTime = checkpoint.value("startTime").toDateTime();
    
    return loadCheckpointFile();
}

void StageTask::updateProgress(int percentage)
{
    if (percentage != m_progress && percentage >= 0 && percentage <= 100) {
        setProgress(percentage);
    }
}

void StageTask::updateStatus(TaskStatus status)
{
    setStatus(status);
}

void StageTask::setProgress(int percentage)
{
    m_progress = percentage;
    emit progressChanged(m_taskId, percentage);
    
    // Save checkpoint periodically
    if (percentage % 10 == 0) {
        saveProgressCheckpoint();
    }
}

void StageTask::setStatus(TaskStatus status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged(m_taskId, status);
    }
}

bool StageTask::checkCancellation() const
{
    return m_isCancelled;
}

bool StageTask::checkPause()
{
    if (m_isPaused) {
        return waitForResume();
    }
    return false;
}

void StageTask::saveProgressCheckpoint()
{
    QVariantMap executionData;
    executionData["progress"] = m_progress;
    executionData["timestamp"] = QDateTime::currentDateTime();
    
    saveExecutionData(executionData);
    saveCheckpoint();
}

void StageTask::saveExecutionData(const QVariantMap &data)
{
    QMutexLocker locker(&m_checkpointMutex);
    
    // Merge with existing checkpoint data
    for (auto it = data.begin(); it != data.end(); ++it) {
        m_checkpointData[it.key()] = it.value();
    }
}

QVariantMap StageTask::loadExecutionData() const
{
    QMutexLocker locker(&m_checkpointMutex);
    return m_checkpointData;
}

void StageTask::executeInternal()
{
    try {
        // Check for cancellation before starting
        if (checkCancellation()) {
            return;
        }
        
        // Initialize execution
        if (!initializeExecution()) {
            emit taskError(m_taskId, "Failed to initialize task execution");
            setStatus(TaskStatus::Failed);
            return;
        }
        
        // Check for pause/cancellation
        if (checkCancellation()) {
            return;
        }
        checkPause();
        
        // Execute main stage logic
        QVariantMap results = executeStage();
        
        // Check for cancellation after execution
        if (checkCancellation()) {
            return;
        }
        
        // Finalize execution
        finalizeExecution();
        
        // Mark as completed
        m_endTime = QDateTime::currentDateTime();
        setStatus(TaskStatus::Completed);
        setProgress(100);
        
        emit taskCompleted(m_taskId, results);
        
    } catch (const std::exception &e) {
        qCWarning(stageTask) << "Task execution failed:" << m_taskId << e.what();
        emit taskError(m_taskId, QString("Task execution failed: %1").arg(e.what()));
        setStatus(TaskStatus::Failed);
    } catch (...) {
        qCWarning(stageTask) << "Task execution failed with unknown error:" << m_taskId;
        emit taskError(m_taskId, "Task execution failed with unknown error");
        setStatus(TaskStatus::Failed);
    }
}

bool StageTask::waitForResume()
{
    QMutexLocker locker(&m_pauseMutex);
    
    while (m_isPaused && !m_isCancelled) {
        m_pauseCondition.wait(&m_pauseMutex);
    }
    
    return !m_isCancelled;
}

void StageTask::updateCheckpointFile() const
{
    if (m_checkpointFilePath.isEmpty()) {
        return;
    }
    
    QFile file(m_checkpointFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc = QJsonDocument::fromVariant(m_checkpointData);
        file.write(doc.toJson());
        file.close();
    } else {
        qCWarning(stageTask) << "Failed to save checkpoint file:" << m_checkpointFilePath;
    }
}

bool StageTask::loadCheckpointFile()
{
    if (m_checkpointFilePath.isEmpty()) {
        return false;
    }
    
    QFile file(m_checkpointFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull()) {
        m_checkpointData = doc.toVariant().toMap();
        return true;
    }
    
    return false;
}

void StageTask::initializeCheckpointFile()
{
    QString checkpointDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/vectordb/checkpoints";
    QDir dir;
    if (!dir.exists(checkpointDir)) {
        dir.mkpath(checkpointDir);
    }
    
    m_checkpointFilePath = QString("%1/%2.checkpoint").arg(checkpointDir, m_taskId);
}