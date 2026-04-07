// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "stagebasedtaskmanager.h"
#include "tasks/dataparsertask.h"
#include "tasks/embeddingtask.h"
#include "tasks/indexbuildtask.h"
#include "tasks/searchtask.h"
#include "tasks/persistencetask.h"
#include "stagetask.h"
#include "workfloworchestrator.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QUuid>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(stageBasedTaskManager, "vectordb.task.manager")

StageBasedTaskManager::StageBasedTaskManager()
    : QObject(nullptr)
    , m_globalSchedulerTimer(new QTimer(this))
{
    qCDebug(stageBasedTaskManager) << "StageBasedTaskManager created";
    
    initializeStageQueues();
    initializeScheduler();
}

StageBasedTaskManager::~StageBasedTaskManager()
{
    qCDebug(stageBasedTaskManager) << "StageBasedTaskManager destroyed";
    cleanupCompletedTasks();
}

QString StageBasedTaskManager::createDataParserTask(const QVariantMap &params, StagePriority priority)
{
    qCDebug(stageBasedTaskManager) << "Creating data parser task";
    
    QString taskId = generateTaskId();
    
    // Create data parser task
    QSharedPointer<StageTask> task = createStageTask(ProcessingStage::DataParsing, taskId, params);
    if (!task) {
        qCWarning(stageBasedTaskManager) << "Failed to create data parser task";
        return QString();
    }
    
    // Store task
    m_stageTasks[taskId] = task;
    
    // Add to queue
    QQueue<QString> &queue = getStageQueue(ProcessingStage::DataParsing, priority);
    queue.enqueue(taskId);
    
    emit stageQueueStatusChanged(ProcessingStage::DataParsing, queue.size(), 0);
    
    return taskId;
}

QString StageBasedTaskManager::createEmbeddingTask(const QVariantMap &params, StagePriority priority)
{
    qCDebug(stageBasedTaskManager) << "Creating embedding task";
    
    QString taskId = generateTaskId();
    
    // Create embedding task
    QSharedPointer<StageTask> task = createStageTask(ProcessingStage::EmbeddingGeneration, taskId, params);
    if (!task) {
        qCWarning(stageBasedTaskManager) << "Failed to create embedding task";
        return QString();
    }
    
    // Store task
    m_stageTasks[taskId] = task;
    
    // Add to queue
    QQueue<QString> &queue = getStageQueue(ProcessingStage::EmbeddingGeneration, priority);
    queue.enqueue(taskId);
    
    emit stageQueueStatusChanged(ProcessingStage::EmbeddingGeneration, queue.size(), 0);
    
    return taskId;
}

QString StageBasedTaskManager::createIndexBuildTask(const QVariantMap &params, StagePriority priority)
{
    qCDebug(stageBasedTaskManager) << "Creating index build task";
    
    QString taskId = generateTaskId();
    
    // Create index build task
    QSharedPointer<StageTask> task = createStageTask(ProcessingStage::IndexBuilding, taskId, params);
    if (!task) {
        qCWarning(stageBasedTaskManager) << "Failed to create index build task";
        return QString();
    }
    
    // Store task
    m_stageTasks[taskId] = task;
    
    // Add to queue
    QQueue<QString> &queue = getStageQueue(ProcessingStage::IndexBuilding, priority);
    queue.enqueue(taskId);
    
    emit stageQueueStatusChanged(ProcessingStage::IndexBuilding, queue.size(), 0);
    
    return taskId;
}

QString StageBasedTaskManager::createSearchTask(const QVariantMap &params, StagePriority priority)
{
    qCDebug(stageBasedTaskManager) << "Creating search task";
    
    QString taskId = generateTaskId();
    
    // Create search task
    QSharedPointer<StageTask> task = createStageTask(ProcessingStage::Search, taskId, params);
    if (!task) {
        qCWarning(stageBasedTaskManager) << "Failed to create search task";
        return QString();
    }
    
    // Store task
    m_stageTasks[taskId] = task;
    
    // Add to queue
    QQueue<QString> &queue = getStageQueue(ProcessingStage::Search, priority);
    queue.enqueue(taskId);
    
    emit stageQueueStatusChanged(ProcessingStage::Search, queue.size(), 0);
    
    return taskId;
}

QString StageBasedTaskManager::createPersistenceTask(const QVariantMap &data, StagePriority priority)
{
    qCDebug(stageBasedTaskManager) << "Creating persistence task";
    
    QString taskId = generateTaskId();
    
    // Create persistence task
    QSharedPointer<StageTask> task = createStageTask(ProcessingStage::DataPersistence, taskId, data);
    if (!task) {
        qCWarning(stageBasedTaskManager) << "Failed to create persistence task";
        return QString();
    }
    
    // Store task
    m_stageTasks[taskId] = task;
    
    // Add to queue
    QQueue<QString> &queue = getStageQueue(ProcessingStage::DataPersistence, priority);
    queue.enqueue(taskId);
    
    emit stageQueueStatusChanged(ProcessingStage::DataPersistence, queue.size(), 0);
    
    return taskId;
}

bool StageBasedTaskManager::pauseWorkflow(const QString &workflowId)
{
    qCDebug(stageBasedTaskManager) << "Pausing workflow:" << workflowId;
    
    auto workflow = m_workflows.value(workflowId);
    if (workflow) {
        return workflow->pauseWorkflow();
    }
    
    return false;
}

bool StageBasedTaskManager::resumeWorkflow(const QString &workflowId)
{
    qCDebug(stageBasedTaskManager) << "Resuming workflow:" << workflowId;
    
    auto workflow = m_workflows.value(workflowId);
    if (workflow) {
        return workflow->resumeWorkflow();
    }
    
    return false;
}

bool StageBasedTaskManager::cancelWorkflow(const QString &workflowId)
{
    qCDebug(stageBasedTaskManager) << "Cancelling workflow:" << workflowId;
    
    auto workflow = m_workflows.value(workflowId);
    if (workflow) {
        return workflow->cancelWorkflow();
    }
    
    return false;
}

void StageBasedTaskManager::setStageThreadCount(ProcessingStage stage, int threadCount)
{
    qCDebug(stageBasedTaskManager) << "Setting thread count for stage:" << static_cast<int>(stage) << "count:" << threadCount;
    
    auto stageQueue = m_stageQueues.value(stage);
    if (stageQueue && stageQueue->threadPool) {
        stageQueue->threadPool->setMaxThreadCount(threadCount);
    }
}

void StageBasedTaskManager::setStageResourceLimit(ProcessingStage stage, const QString &resourceType, qreal limit)
{
    qCDebug(stageBasedTaskManager) << "Setting resource limit for stage:" << static_cast<int>(stage) 
                                   << "resource:" << resourceType << "limit:" << limit;
    
    m_stageResourceLimits[stage][resourceType] = limit;
}

int StageBasedTaskManager::getStageQueueLength(ProcessingStage stage) const
{
    auto stageQueue = m_stageQueues.value(stage);
    if (!stageQueue) {
        return 0;
    }
    
    QMutexLocker locker(&stageQueue->queueMutex);
    return stageQueue->criticalQueue.size() + stageQueue->highQueue.size() + 
           stageQueue->normalQueue.size() + stageQueue->lowQueue.size();
}

QVariantMap StageBasedTaskManager::getStageStatistics(ProcessingStage stage) const
{
    QVariantMap stats;
    
    stats["stage"] = static_cast<int>(stage);
    stats["queueLength"] = getStageQueueLength(stage);
    stats["runningTasks"] = m_runningTasksByStage.value(stage).size();
    stats["resourceUsage"] = m_stageResourceUsage.value(stage);
    stats["resourceLimits"] = m_stageResourceLimits.value(stage);
    
    return stats;
}

QVariantMap StageBasedTaskManager::getTaskInfo(const QString &taskId) const
{
    auto task = m_stageTasks.value(taskId);
    if (task) {
        QVariantMap info;
        info["taskId"] = taskId;
        info["stage"] = static_cast<int>(task->getStage());
        info["status"] = static_cast<int>(task->getStatus());
        info["progress"] = task->getProgress();
        info["startTime"] = task->getStartTime();
        info["endTime"] = task->getEndTime();
        info["resourceRequirements"] = task->getResourceRequirements();
        info["estimatedDuration"] = task->getEstimatedDuration();
        info["resourceType"] = task->getResourceType();
        info["status"] = "pending"; // Placeholder
        return info;
    }
    
    return QVariantMap();
}

QVariantMap StageBasedTaskManager::getWorkflowInfo(const QString &workflowId) const
{
    auto workflow = m_workflows.value(workflowId);
    if (workflow) {
        return workflow->getWorkflowCheckpoint();
    }
    
    return QVariantMap();
}

QVariantList StageBasedTaskManager::getActiveTasksByStage(ProcessingStage stage) const
{
    QVariantList tasks;
    
    const QSet<QString> &runningTasks = m_runningTasksByStage.value(stage);
    for (const QString &taskId : runningTasks) {
        tasks.append(taskId);
    }
    
    return tasks;
}

bool StageBasedTaskManager::cancelTask(const QString &taskId)
{
    qCDebug(stageBasedTaskManager) << "Cancelling task:" << taskId;
    
    auto task = m_stageTasks.value(taskId);
    if (task) {
        task->cancel();
        return true;
    }
    
    return false;
}

void StageBasedTaskManager::enableRandomExecution(ProcessingStage stage, bool enabled)
{
    auto stageQueue = m_stageQueues.value(stage);
    if (stageQueue) {
        QMutexLocker locker(&stageQueue->queueMutex);
        stageQueue->randomExecutionEnabled = enabled;
    }
}

void StageBasedTaskManager::setStageSchedulingStrategy(ProcessingStage stage, const QString &strategy)
{
    auto stageQueue = m_stageQueues.value(stage);
    if (stageQueue) {
        QMutexLocker locker(&stageQueue->queueMutex);
        stageQueue->schedulingStrategy = strategy;
    }
}

void StageBasedTaskManager::onStageTaskCompleted(const QString &taskId, const QVariantMap &results)
{
    qCDebug(stageBasedTaskManager) << "Stage task completed:" << taskId << "with results:" << results.keys();
    
    auto task = m_stageTasks.value(taskId);
    if (task) {
        ProcessingStage stage = task->getStage();
        
        // Emit completion signal with results
        emit stageTaskCompleted(taskId, stage, true, results);
        
        // Remove from running tasks
        m_runningTasksByStage[stage].remove(taskId);
        
        // Remove from task map
        m_stageTasks.remove(taskId);
        
        // Schedule next task for this stage
        scheduleNextStageTask(stage);
    }
}

void StageBasedTaskManager::onStageTaskError(const QString &taskId, const QString &error)
{
    qCWarning(stageBasedTaskManager) << "Stage task error:" << taskId << error;
    
    auto task = m_stageTasks.value(taskId);
    if (task) {
        ProcessingStage stage = task->getStage();
        emit stageTaskCompleted(taskId, stage, false, QVariantMap());
        
        // Remove from running tasks
        m_runningTasksByStage[stage].remove(taskId);
        
        // Remove from task map
        m_stageTasks.remove(taskId);
        
        // Schedule next task for this stage
        scheduleNextStageTask(stage);
    }
}

void StageBasedTaskManager::onResourceUsageChanged(ProcessingStage stage, const QString &resourceType, qreal usage)
{
    m_stageResourceUsage[stage][resourceType] = usage;
}

void StageBasedTaskManager::processStageQueues()
{
    // Process all stage queues
    for (auto it = m_stageQueues.begin(); it != m_stageQueues.end(); ++it) {
        scheduleNextStageTask(it.key());
    }
    
    // Process workflow steps
    processWorkflowSteps();
    
    // Cleanup completed tasks
    cleanupCompletedTasks();
}

QString StageBasedTaskManager::generateTaskId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString StageBasedTaskManager::generateWorkflowId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QQueue<QString>& StageBasedTaskManager::getStageQueue(ProcessingStage stage, StagePriority priority)
{
    auto stageQueue = m_stageQueues.value(stage);
    if (!stageQueue) {
        // This should not happen if properly initialized
        static QQueue<QString> emptyQueue;
        return emptyQueue;
    }
    
    switch (priority) {
    case StagePriority::Critical:
        return stageQueue->criticalQueue;
    case StagePriority::High:
        return stageQueue->highQueue;
    case StagePriority::Normal:
        return stageQueue->normalQueue;
    case StagePriority::Low:
        return stageQueue->lowQueue;
    }
    
    return stageQueue->normalQueue;
}

void StageBasedTaskManager::scheduleNextStageTask(ProcessingStage stage)
{
    auto stageQueue = m_stageQueues.value(stage);
    if (!stageQueue) {
        return;
    }
    
    // Check if we can start more tasks for this stage
    if (!canStartStageTask(stage, nullptr)) {
        return;
    }
    
    QString nextTaskId = selectNextTaskFromQueue(stage);
    if (nextTaskId.isEmpty()) {
        return;
    }
    
    // Get the task and start it
    auto task = m_stageTasks.value(nextTaskId);
    if (task) {
        // Add to running tasks
        m_runningTasksByStage[stage].insert(nextTaskId);
        
        // Start the task in the appropriate thread pool
        auto stageQueue = m_stageQueues.value(stage);
        if (stageQueue && stageQueue->threadPool) {
            TaskRunner *runner = task->createRunner();
            stageQueue->threadPool->start(runner);
        }
        
        emit stageTaskStarted(nextTaskId, stage);
    } else {
        qCWarning(stageBasedTaskManager) << "Task not found for execution:" << nextTaskId;
    }
}

bool StageBasedTaskManager::canStartStageTask(ProcessingStage stage, const QSharedPointer<StageTask> &task) const
{
    Q_UNUSED(task)
    
    auto stageQueue = m_stageQueues.value(stage);
    if (!stageQueue || !stageQueue->threadPool) {
        return false;
    }
    
    // Check if thread pool has capacity
    return stageQueue->threadPool->activeThreadCount() < stageQueue->threadPool->maxThreadCount();
}

QString StageBasedTaskManager::selectNextTaskFromQueue(ProcessingStage stage)
{
    auto stageQueue = m_stageQueues.value(stage);
    if (!stageQueue) {
        return QString();
    }
    
    QMutexLocker locker(&stageQueue->queueMutex);
    
    // Select from highest priority queue first
    if (!stageQueue->criticalQueue.isEmpty()) {
        return stageQueue->criticalQueue.dequeue();
    }
    if (!stageQueue->highQueue.isEmpty()) {
        return stageQueue->highQueue.dequeue();
    }
    if (!stageQueue->normalQueue.isEmpty()) {
        return stageQueue->normalQueue.dequeue();
    }
    if (!stageQueue->lowQueue.isEmpty()) {
        return stageQueue->lowQueue.dequeue();
    }
    
    return QString();
}

void StageBasedTaskManager::updateStageResourceUsage(ProcessingStage stage)
{
    // Calculate current resource usage for the stage
    QVariantMap usage;
    usage["cpu"] = 0.0;
    usage["memory"] = 0.0;
    usage["gpu"] = 0.0;
    usage["network"] = 0.0;
    usage["io"] = 0.0;
    
    // Sum up resource usage from running tasks
    const QSet<QString> &runningTasks = m_runningTasksByStage.value(stage);
    for (const QString &taskId : runningTasks) {
        auto task = m_stageTasks.value(taskId);
        if (task) {
            QVariantMap requirements = task->getResourceRequirements();
            for (auto it = requirements.constBegin(); it != requirements.constEnd(); ++it) {
                qreal currentUsage = usage.value(it.key()).toReal();
                qreal taskUsage = it.value().toReal();
                usage[it.key()] = currentUsage + taskUsage;
            }
        }
    }
    
    m_stageResourceUsage[stage] = usage;
}

void StageBasedTaskManager::cleanupCompletedTasks()
{
    // Remove completed and failed tasks from tracking
    QStringList tasksToRemove;
    
    for (auto it = m_stageTasks.constBegin(); it != m_stageTasks.constEnd(); ++it) {
        const QString &taskId = it.key();
        const auto &task = it.value();
        
        if (task->getStatus() == StageTask::TaskStatus::Completed ||
            task->getStatus() == StageTask::TaskStatus::Failed ||
            task->getStatus() == StageTask::TaskStatus::Cancelled) {
            
            tasksToRemove.append(taskId);
            
            // Remove from running tasks
            for (auto &runningTasks : m_runningTasksByStage) {
                runningTasks.remove(taskId);
            }
        }
    }
    
    // Remove completed tasks
    for (const QString &taskId : tasksToRemove) {
        m_stageTasks.remove(taskId);
    }
    
    if (!tasksToRemove.isEmpty()) {
        qCDebug(stageBasedTaskManager) << "Cleaned up" << tasksToRemove.size() << "completed tasks";
    }
}

void StageBasedTaskManager::processWorkflowSteps()
{
    // Process active workflows
    for (auto it = m_workflows.constBegin(); it != m_workflows.constEnd(); ++it) {
        const QString &workflowId = it.key();
        const auto &orchestrator = it.value();
        
        if (orchestrator) {
            // Check if workflow needs to advance to next step
            // This is a simplified implementation
            qCDebug(stageBasedTaskManager) << "Processing workflow step:" << workflowId;
        }
    }
}

void StageBasedTaskManager::advanceWorkflow(const QString &workflowId, const QString &completedTaskId)
{
    Q_UNUSED(workflowId)
    Q_UNUSED(completedTaskId)
    
    // Find the workflow and advance it to the next step
    auto orchestrator = m_workflows.value(workflowId);
    if (orchestrator) {
        qCDebug(stageBasedTaskManager) << "Advancing workflow:" << workflowId << "after task completion:" << completedTaskId;
        
        // This is a simplified implementation
        // In a real implementation, the orchestrator would manage the workflow state
        // and determine what the next step should be
    } else {
        qCWarning(stageBasedTaskManager) << "Workflow not found for advancement:" << workflowId;
    }
}

void StageBasedTaskManager::initializeStageQueues()
{
    // Initialize queues for each processing stage
    QList<ProcessingStage> stages = {
        ProcessingStage::DataParsing,
        ProcessingStage::EmbeddingGeneration,
        ProcessingStage::IndexBuilding,
        ProcessingStage::DataPersistence,
        ProcessingStage::Search
    };
    
    for (ProcessingStage stage : stages) {
        QSharedPointer<StageQueue> stageQueue;
        stageQueue = QSharedPointer<StageQueue>::create();
        stageQueue->threadPool.reset(new QThreadPool());
        
        // Set default thread counts based on stage type
        switch (stage) {
        case ProcessingStage::DataParsing:
            stageQueue->threadPool->setMaxThreadCount(4);
            break;
        case ProcessingStage::EmbeddingGeneration:
            stageQueue->threadPool->setMaxThreadCount(2);
            break;
        case ProcessingStage::IndexBuilding:
            stageQueue->threadPool->setMaxThreadCount(2);
            break;
        case ProcessingStage::DataPersistence:
            stageQueue->threadPool->setMaxThreadCount(4);
            break;
        case ProcessingStage::Search:
            stageQueue->threadPool->setMaxThreadCount(8);
            break;
        }
        
        m_stageQueues.insert(stage, stageQueue);
    }
}

void StageBasedTaskManager::initializeScheduler()
{
    m_globalSchedulerTimer->setInterval(1000); // 1 second
    connect(m_globalSchedulerTimer, &QTimer::timeout, this, &StageBasedTaskManager::processStageQueues);
    m_globalSchedulerTimer->start();
}

QSharedPointer<StageTask> StageBasedTaskManager::createStageTask(ProcessingStage stage, const QString &taskId, const QVariantMap &params)
{
    QSharedPointer<StageTask> task;
    
    switch (stage) {
    case ProcessingStage::DataParsing:
        task = QSharedPointer<DataParserTask>::create(taskId, params);
        break;
    case ProcessingStage::EmbeddingGeneration:
        task = QSharedPointer<EmbeddingTask>::create(taskId, params);
        break;
    case ProcessingStage::IndexBuilding:
        task = QSharedPointer<IndexBuildTask>::create(taskId, params);
        break;
    case ProcessingStage::Search: {
        task = QSharedPointer<SearchTask>::create(taskId, params);
        break;
    }
    case ProcessingStage::DataPersistence: {
        task = QSharedPointer<PersistenceTask>::create(taskId, params);
        break;
    }
    }
    
    if (task) {
        // Connect task signals
        connect(task.data(), &StageTask::taskCompleted,
                this, &StageBasedTaskManager::onStageTaskCompleted);
        connect(task.data(), &StageTask::taskError,
                this, &StageBasedTaskManager::onStageTaskError);
        connect(task.data(), &StageTask::progressChanged,
                this, &StageBasedTaskManager::stageTaskProgress);
    }
    
    return task;
}
