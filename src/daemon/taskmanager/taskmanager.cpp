// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "taskmanager.h"
#include "threadpool/threadpoolmanager.h"
#include "modelcenter/modelhub/modelhubwrapper.h"
#include "common/aitypes.h"

#include <QMutexLocker>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>

Q_DECLARE_LOGGING_CATEGORY(logTaskManager)

AIDAEMON_USE_NAMESPACE

TaskManager::TaskManager(QObject *parent)
    : QObject(parent)
    , pendingDispatchCount(0)

{
    // Initialize default concurrency limits (only for local models and fallback)
    defaultConcurrencyLimits = {
        // Local model limits (hardware constrained)
        {{TaskType::Chat, ModelDeployType::Local}, 2},
        {{TaskType::FunctionCalling, ModelDeployType::Local}, 1},
        {{TaskType::ImageGeneration, ModelDeployType::Local}, 1},
        {{TaskType::TextToSpeech, ModelDeployType::Local}, 2},
        {{TaskType::SpeechToText, ModelDeployType::Local}, 2},
        {{TaskType::OCR, ModelDeployType::Local}, 3},
        {{TaskType::TextEmbedding, ModelDeployType::Local}, 4},
        {{TaskType::VectorSearch, ModelDeployType::Local}, 6}
    };
    
    // No longer use polling timer - tasks will be dispatched immediately when added
    
    // Set up metrics timer
    metricsTimer = new QTimer(this);
    connect(metricsTimer, &QTimer::timeout, this, &TaskManager::updateMetrics);
    metricsTimer->start(60000); // Update statistics every 1 minute
    
    qCInfo(logTaskManager) << "TaskManager initialized with" << defaultConcurrencyLimits.size() << "concurrency limits";
}

TaskManager::~TaskManager()
{
    // Stop metrics timer
    if (metricsTimer) {
        metricsTimer->stop();
    }
    
    // Cancel all pending tasks
    QMutexLocker locker(&taskMutex);
    for (auto& queue : waitingQueues) {
        while (!queue.isEmpty()) {
            Task task = queue.dequeue();
            allTasks[task.taskId].status = TaskStatus::Cancelled;
            emit taskCancelled(task.taskId);
        }
    }
    
    qCInfo(logTaskManager) << "TaskManager destroyed";
}

// Static task ID generation for Task struct
quint64 TaskManager::Task::generateUniqueTaskId()
{
    static QAtomicInt taskIdCounter(1000);  // Start from 1000, same as before
    return static_cast<quint64>(taskIdCounter.fetchAndAddOrdered(1));
}

quint64 TaskManager::generateTaskId()
{
    // This method is now deprecated, but kept for compatibility
    // New tasks should use Task constructor which auto-generates ID
    return Task::generateUniqueTaskId();
}

void TaskManager::addTask(const Task& task)
{
    TaskType taskType;
    {
        QMutexLocker locker(&taskMutex);
        
        // Create a mutable copy for modification
        Task newTask = task;
        newTask.status = TaskStatus::Pending;
        // Note: taskId is already set by Task constructor, no need to reassign
        
        // Auto-detect deployment type if not specified
        if (newTask.deployType == ModelDeployType::Hybrid) {
            newTask.deployType = detectModelDeployType(newTask.providerName, newTask.modelName);
        }
        
        allTasks[newTask.taskId] = newTask;
        waitingQueues[newTask.taskType].enqueue(newTask);
        taskType = newTask.taskType;
        
        qCInfo(logTaskManager) << "Added task:" << newTask.taskId 
                              << "type:" << taskTypeToString(newTask.taskType)
                              << "deploy:" << static_cast<int>(newTask.deployType);
        
        emit queueSizeChanged(newTask.taskType, waitingQueues[newTask.taskType].size());
    }
    
    // Immediately trigger task dispatch (event-driven, not polling)
    QMetaObject::invokeMethod(this, "dispatchTasks", Qt::QueuedConnection);
}

void TaskManager::dispatchTasks()
{
    // Debounce mechanism: if another dispatch is already pending, skip this one
    int currentCount = pendingDispatchCount.fetchAndAddOrdered(1);
    if (currentCount > 0) {
        pendingDispatchCount.fetchAndSubOrdered(1);
        return; // Another dispatch is already running or pending
    }
    
    // Step 1: Collect tasks to execute (single lock point)
    QList<Task> tasksToExecute = collectTasksToExecute();
    
    // Step 2: Execute tasks asynchronously
    for (const Task& task : tasksToExecute) {
        executeTaskAsync(task);
    }
    
    // Step 3: Update queue size signals and reset counter
    {
        QMutexLocker locker(&taskMutex);
        for (auto it = waitingQueues.begin(); it != waitingQueues.end(); ++it) {
            TaskType type = it.key();
            const auto& queue = it.value();
            emit queueSizeChanged(type, queue.size());
        }
    }
    
    // Reset pending dispatch counter
    pendingDispatchCount.storeRelease(0);
}

// Robust model deployment type detection - only check for local models
TaskManager::ModelDeployType TaskManager::detectModelDeployType(const QString& providerName, const QString& modelName) const
{
    // Strategy: Only definitively identify local models, treat everything else as cloud
    // This approach is more reliable and maintainable than hardcoding provider/model lists
    
    try {
        // Method 1: Check if provider is explicitly "modelhub" (most reliable)
        if (providerName.compare("modelhub", Qt::CaseInsensitive) == 0) {
            // Double-check if the model is actually installed locally
            if (ModelhubWrapper::isModelInstalled(modelName)) {
                qCDebug(logTaskManager) << "Detected local model via modelhub provider:" 
                                       << modelName << "is installed";
                return ModelDeployType::Local;
            } else {
                qCWarning(logTaskManager) << "Modelhub provider specified but model not installed:" 
                                        << modelName;
                return ModelDeployType::Cloud; // Fallback to cloud
            }
        }
        
        // Method 2: Direct model installation check (covers edge cases)
        // This handles cases where provider name might not be "modelhub" but model is actually local
        if (!modelName.isEmpty() && ModelhubWrapper::isModelInstalled(modelName)) {
            qCDebug(logTaskManager) << "Detected local model via installation check:" 
                                   << modelName << "with provider:" << providerName;
            return ModelDeployType::Local;
        }
        
        // Method 3: All other cases are cloud/online models
        qCDebug(logTaskManager) << "Treating as cloud model - Provider:" 
                               << providerName << "Model:" << modelName;
        return ModelDeployType::Cloud;
        
    } catch (const std::exception& e) {
        qCWarning(logTaskManager) << "Error detecting model deployment type:" 
                                 << e.what() 
                                 << "- defaulting to cloud for safety";
        return ModelDeployType::Cloud; // Safe fallback
    }
}

// Utility method to get all installed local models (for caching or validation)
QStringList TaskManager::getInstalledLocalModels() const
{
    try {
        return ModelhubWrapper::installedModels();
    } catch (const std::exception& e) {
        qCWarning(logTaskManager) << "Failed to get installed models:" << e.what();
        return QStringList();
    }
}

// Utility method to check if modelhub system is available
bool TaskManager::isLocalModelSystemAvailable() const
{
    try {
        return ModelhubWrapper::isModelhubInstalled();
    } catch (const std::exception& e) {
        qCWarning(logTaskManager) << "Failed to check modelhub availability:" << e.what();
        return false;
    }
}

// Responsive throttling implementation - "Increase on success, decrease on failure"
void TaskManager::adjustConcurrencyBasedOnResponse(TaskType taskType, ModelDeployType deployType, bool success, int errorCode)
{
    if (deployType != ModelDeployType::Cloud) {
        return; // Only apply to cloud APIs
    }
    
    auto key = qMakePair(taskType, deployType);
    auto& state = throttlingStates[key];
    
    if (!state.enabled) {
        return;
    }
    
    QMutexLocker locker(&taskMutex);
    QDateTime now = QDateTime::currentDateTime();
    
    // Check cooldown period to prevent oscillation
    if (state.lastAdjustment.isValid() && 
        state.lastAdjustment.msecsTo(now) < state.cooldownMs) {
        return;
    }
    
    if (success) {
        // Reset failure counter on success
        state.consecutiveFailures = 0;
        state.consecutiveSuccesses++;
        
        // Increase concurrency after sustained success
        if (state.consecutiveSuccesses >= state.successThreshold &&
            state.currentConcurrency < state.maxConcurrency) {
            
            int newConcurrency = qMin(state.maxConcurrency, 
                                    static_cast<int>(state.currentConcurrency * state.increaseRate));
            
            if (newConcurrency > state.currentConcurrency) {
                qCInfo(logTaskManager) << "Increasing concurrency for" 
                                      << taskTypeToString(taskType) 
                                      << "from" << state.currentConcurrency 
                                      << "to" << newConcurrency;
                
                state.currentConcurrency = newConcurrency;
                state.consecutiveSuccesses = 0; // Reset counter
                state.lastAdjustment = now;
            }
        }
    } else {
        // Handle failure cases
        if (errorCode == 429) { // HTTP 429 Too Many Requests
            state.consecutiveSuccesses = 0;
            state.consecutiveFailures++;
            
            // Immediate aggressive reduction on 429
            int newConcurrency = qMax(state.minConcurrency,
                                    static_cast<int>(state.currentConcurrency * state.decreaseRate));
            
            if (newConcurrency < state.currentConcurrency) {
                qCWarning(logTaskManager) << "HTTP 429 detected - reducing concurrency for"
                                        << taskTypeToString(taskType)
                                        << "from" << state.currentConcurrency
                                        << "to" << newConcurrency;
                
                state.currentConcurrency = newConcurrency;
                state.lastAdjustment = now;
                
                // More aggressive reduction for consecutive failures
                if (state.consecutiveFailures >= 3) {
                    state.decreaseRate = qMax(0.5, state.decreaseRate - 0.1);
                }
            }
        } else {
            // Other errors don't affect throttling
            state.consecutiveFailures = 0;
        }
    }
}

// Check if a task can be dispatched based on current concurrency limits
bool TaskManager::canDispatchTask(TaskType taskType, ModelDeployType deployType) const
{
    auto key = qMakePair(taskType, deployType);
    int currentRunning = currentTasksByDeploy.value(key).loadAcquire();
    
    if (deployType == ModelDeployType::Cloud) {
        // Use responsive throttling for cloud APIs
        const auto& state = throttlingStates.value(key);
        if (state.enabled) {
            return currentRunning < state.currentConcurrency;
        }
        // If responsive throttling is disabled, allow unlimited for cloud
        return true;
    } else {
        // Use static limits for local models
        int maxConcurrent = defaultConcurrencyLimits.value(key, 4); // Default fallback
        return currentRunning < maxConcurrent;
    }
}

// Public interface for reporting task results
void TaskManager::reportTaskResult(TaskType taskType, ModelDeployType deployType, bool success, int errorCode)
{
    adjustConcurrencyBasedOnResponse(taskType, deployType, success, errorCode);
    updateThrottlingMetrics(taskType, deployType);
}

// Enable/disable responsive throttling for specific task types
void TaskManager::enableResponsiveThrottling(TaskType taskType, ModelDeployType deployType, bool enabled)
{
    if (deployType != ModelDeployType::Cloud) {
        return; // Only applicable to cloud APIs
    }
    
    auto key = qMakePair(taskType, deployType);
    auto& state = throttlingStates[key];
    state.enabled = enabled;
    
    if (enabled) {
        qCInfo(logTaskManager) << "Enabled responsive throttling for" 
                              << taskTypeToString(taskType) 
                              << "cloud APIs";
    }
}

// Update throttling metrics and cleanup old data
void TaskManager::updateThrottlingMetrics(TaskType taskType, ModelDeployType deployType)
{
    if (deployType != ModelDeployType::Cloud) {
        return;
    }
    
    auto key = qMakePair(taskType, deployType);
    auto& state = throttlingStates[key];
    
    QDateTime now = QDateTime::currentDateTime();
    state.recentRequests.enqueue(now);
    
    // Remove requests older than 1 minute for rate calculation
    while (!state.recentRequests.isEmpty() && 
           state.recentRequests.head().secsTo(now) > 60) {
        state.recentRequests.dequeue();
    }
}

// Task type to thread pool type mapping
ThreadPoolManager::PoolType TaskManager::getPoolTypeForTask(TaskType taskType) const
{
    switch (taskType) {
        // CPU-intensive tasks
        case TaskType::TextEmbedding:
        case TaskType::VectorSearch:
        case TaskType::SemanticSearch:
        case TaskType::ImageGeneration:
        case TaskType::ImageEditing:
            return ThreadPoolManager::PoolType::CPUBound;
            
        // General tasks
        default:
            return ThreadPoolManager::PoolType::General;
    }
}

void TaskManager::updateMetrics()
{
    // Placeholder for periodic metrics updates
    qCDebug(logTaskManager) << "Updating task metrics - Total pending:" << getTotalPendingCount()
                           << "Total running:" << getTotalRunningCount();
}

void TaskManager::updateTaskMetrics(const Task& task, bool success)
{
    // Update execution time statistics
    if (task.startedAt.isValid() && task.completedAt.isValid()) {
        qint64 executionTime = task.startedAt.msecsTo(task.completedAt);
        executionTimes[task.taskType].append(executionTime);
        
        // Keep only recent 100 measurements per task type
        if (executionTimes[task.taskType].size() > 100) {
            executionTimes[task.taskType].removeFirst();
        }
    }
    
    // Update success/failure counts
    auto& counts = successFailCounts[task.taskType];
    if (success) {
        counts.first++; // success count
    } else {
        counts.second++; // failure count
    }
}

// Statistics implementation
int TaskManager::getTotalPendingCount() const
{
    QMutexLocker locker(&taskMutex);
    int total = 0;
    for (const auto& queue : waitingQueues) {
        total += queue.size();
    }
    return total;
}

int TaskManager::getTotalRunningCount() const
{
    QMutexLocker locker(&taskMutex);
    return runningTasks.size();
}

double TaskManager::getAverageExecutionTime(TaskType type) const
{
    QMutexLocker locker(&taskMutex);
    const auto& times = executionTimes.value(type);
    if (times.isEmpty()) return 0.0;
    
    qint64 sum = 0;
    for (qint64 time : times) {
        sum += time;
    }
    return static_cast<double>(sum) / times.size();
}

double TaskManager::getSuccessRate(TaskType type) const
{
    QMutexLocker locker(&taskMutex);
    const auto& counts = successFailCounts.value(type);
    int total = counts.first + counts.second;
    return total > 0 ? static_cast<double>(counts.first) / total : 1.0;
}

// Utility function implementations
QString TaskManager::taskTypeToString(TaskType type)
{
    // Use unified AITypes function to avoid duplication
    return AITypes::serviceTypeToString(type);
}

QString TaskManager::taskStatusToString(TaskStatus status)
{
    switch (status) {
        case TaskStatus::Pending: return "Pending";
        case TaskStatus::Running: return "Running";
        case TaskStatus::Completed: return "Completed";
        case TaskStatus::Failed: return "Failed";
        case TaskStatus::Cancelled: return "Cancelled";
        default: return "Unknown";
    }
}

TaskManager::TaskType TaskManager::stringToTaskType(const QString& typeStr)
{
    if (typeStr == "Chat") return TaskType::Chat;
    if (typeStr == "FunctionCalling") return TaskType::FunctionCalling;
    if (typeStr == "OCR") return TaskType::OCR;
    if (typeStr == "ImageGeneration") return TaskType::ImageGeneration;
    if (typeStr == "TextToSpeech") return TaskType::TextToSpeech;
    if (typeStr == "SpeechToText") return TaskType::SpeechToText;
    if (typeStr == "TextEmbedding") return TaskType::TextEmbedding;
    if (typeStr == "VectorSearch") return TaskType::VectorSearch;
    // Add more mappings as needed
    return TaskType::Chat; // Default fallback
}

// Additional method implementations

void TaskManager::cancelTask(quint64 taskId)
{
    QMutexLocker locker(&taskMutex);
    
    auto taskIt = allTasks.find(taskId);
    if (taskIt == allTasks.end()) {
        qCWarning(logTaskManager) << "Cannot cancel non-existent task:" << taskId;
        return;
    }
    
    Task& task = taskIt.value();
    if (task.status == TaskStatus::Running) {
        // Mark for cancellation and let the executor handle it
        cancelledTasks.insert(taskId);
        qCInfo(logTaskManager) << "Marked running task for cancellation:" << taskId;
    } else if (task.status == TaskStatus::Pending) {
        // Remove from waiting queue
        cancelledTasks.insert(taskId);
        task.status = TaskStatus::Cancelled;
        emit taskCancelled(taskId);
        qCInfo(logTaskManager) << "Cancelled pending task:" << taskId;
    }
}

void TaskManager::cancelTasksByType(TaskType type)
{
    QMutexLocker locker(&taskMutex);
    
    // Cancel all pending tasks of this type
    auto& queue = waitingQueues[type];
    while (!queue.isEmpty()) {
        Task task = queue.dequeue();
        cancelledTasks.insert(task.taskId);
        allTasks[task.taskId].status = TaskStatus::Cancelled;
        emit taskCancelled(task.taskId);
    }
    
    // Mark running tasks for cancellation
    for (auto& task : allTasks) {
        if (task.taskType == type && task.status == TaskStatus::Running) {
            cancelledTasks.insert(task.taskId);
        }
    }
    
    qCInfo(logTaskManager) << "Cancelled all tasks of type:" << taskTypeToString(type);
}

void TaskManager::setLocalModelLimit(TaskType type, int maxTasks)
{
    QMutexLocker locker(&taskMutex);
    auto key = qMakePair(type, ModelDeployType::Local);
    defaultConcurrencyLimits[key] = maxTasks;
    qCInfo(logTaskManager) << "Set local model limit for" << taskTypeToString(type) 
                          << "to" << maxTasks;
}

int TaskManager::getCurrentConcurrencyLimit(TaskType taskType, ModelDeployType deployType) const
{
    QMutexLocker locker(&taskMutex);
    auto key = qMakePair(taskType, deployType);
    
    if (deployType == ModelDeployType::Cloud) {
        const auto& state = throttlingStates.value(key);
        return state.enabled ? state.currentConcurrency : 100; // Default high limit
    } else {
        return defaultConcurrencyLimits.value(key, 4); // Default local limit
    }
}

QList<TaskManager::Task> TaskManager::getPendingTasks(TaskType type)
{
    QMutexLocker locker(&taskMutex);
    QList<Task> result;
    
    if (type == static_cast<TaskType>(0)) {
        // Return all pending tasks
        for (const auto& queue : waitingQueues) {
            for (const Task& task : queue) {
                result.append(task);
            }
        }
    } else {
        // Return pending tasks of specific type
        const auto& queue = waitingQueues.value(type);
        for (const Task& task : queue) {
            result.append(task);
        }
    }
    
    return result;
}

QList<TaskManager::Task> TaskManager::getRunningTasks(TaskType type)
{
    QMutexLocker locker(&taskMutex);
    QList<Task> result;
    
    for (const auto& task : allTasks) {
        if (task.status == TaskStatus::Running) {
            if (type == static_cast<TaskType>(0) || task.taskType == type) {
                result.append(task);
            }
        }
    }
    
    return result;
}

TaskManager::Task TaskManager::getTask(quint64 taskId) const
{
    QMutexLocker locker(&taskMutex);
    return allTasks.value(taskId);
}

int TaskManager::getTypeRunningCount(TaskType type) const
{
    QMutexLocker locker(&taskMutex);
    int count = 0;
    for (const auto& task : allTasks) {
        if (task.taskType == type && task.status == TaskStatus::Running) {
            count++;
        }
    }
    return count;
}

QMap<TaskManager::TaskType, int> TaskManager::getAllTypeCounts() const
{
    QMutexLocker locker(&taskMutex);
    QMap<TaskType, int> counts;
    
    // Count pending tasks
    for (auto it = waitingQueues.begin(); it != waitingQueues.end(); ++it) {
        counts[it.key()] += it.value().size();
    }
    
    // Count running tasks
    for (const auto& task : allTasks) {
        if (task.status == TaskStatus::Running) {
            counts[task.taskType]++;
        }
    }
    
    return counts;
}

qint64 TaskManager::getTotalExecutionTime(TaskType type) const
{
    QMutexLocker locker(&taskMutex);
    const auto& times = executionTimes.value(type);
    qint64 total = 0;
    for (qint64 time : times) {
        total += time;
    }
    return total;
}



void TaskManager::handleTaskCompletion(quint64 taskId)
{
    QMutexLocker locker(&taskMutex);
    
    // Remove from running tasks
    runningTasks.remove(taskId);
    
    // Update task status if it's still tracked
    if (allTasks.contains(taskId)) {
        Task& task = allTasks[taskId];
        if (task.status == TaskStatus::Running) {
            task.completedAt = QDateTime::currentDateTime();
            task.status = TaskStatus::Completed;
        }
    }
}

QList<TaskManager::Task> TaskManager::collectTasksToExecute()
{
    QList<Task> tasksToExecute;
    QMutexLocker locker(&taskMutex);
    
    // Process waiting queues by task type
    for (auto it = waitingQueues.begin(); it != waitingQueues.end(); ++it) {
        auto& queue = it.value();
        
        if (queue.isEmpty()) continue;
        
        // Process tasks in queue
        while (!queue.isEmpty()) {
            Task task = queue.head(); // Peek first task
            
            // Check if task was cancelled
            if (cancelledTasks.contains(task.taskId)) {
                queue.dequeue(); // Remove from queue
                cancelledTasks.remove(task.taskId);
                allTasks[task.taskId].status = TaskStatus::Cancelled;
                emit taskCancelled(task.taskId);
                continue;
            }
            
            // Check concurrency limits (responsive throttling for cloud, static for local)
            if (!canDispatchTask(task.taskType, task.deployType)) {
                // Cannot dispatch now, leave task in queue
                break;
            }
            
            // Remove task from queue and mark as ready to execute
            task = queue.dequeue();
            task.startedAt = QDateTime::currentDateTime();
            task.status = TaskStatus::Running;
            allTasks[task.taskId] = task;
            
            // Update deployment-specific counters
            auto deployKey = qMakePair(task.taskType, task.deployType);
            currentTasksByDeploy[deployKey].fetchAndAddOrdered(1);
            
            tasksToExecute.append(task);
            emit taskStarted(task.taskId);
        }
    }
    
    return tasksToExecute;
}

void TaskManager::executeTaskAsync(const Task& task)
{
    auto poolType = getPoolTypeForTask(task.taskType);
    auto future = ThreadPoolManager::getInstance().submitTask(poolType, [this, task]() {
        bool success = false;
        int errorCode = 0;
        QString errorMsg;
        QString errorDetails;
        
        QVariant taskResult;
        try {
            // Execute task without holding any locks
            taskResult = task.executor();
            success = true;
            
            qCInfo(logTaskManager) << "Task completed successfully:" 
                                  << task.taskId 
                                  << taskTypeToString(task.taskType);
            
        } catch (const NetworkException& e) {
            success = false;
            errorCode = e.getErrorCode();
            errorMsg = e.getMessage();
            errorDetails = e.getDetails();
            
            qCWarning(logTaskManager) << "Network error in task:" 
                                     << task.taskId 
                                     << "HTTP" << errorCode 
                                     << errorMsg;
            
            // Handle specific network errors
            if (e.isRateLimited()) {
                qCWarning(logTaskManager) << "Rate limit exceeded for task:" 
                                         << task.taskId 
                                         << "- triggering responsive throttling";
            }
            
        } catch (const ModelException& e) {
            success = false;
            errorCode = e.getErrorCode();
            errorMsg = e.getMessage();
            errorDetails = QString("Model: %1, Type: %2")
                            .arg(e.getModelName())
                            .arg(static_cast<int>(e.getType()));
            
            qCCritical(logTaskManager) << "Model error in task:" 
                                      << task.taskId 
                                      << "Model:" << e.getModelName()
                                      << "Error:" << errorMsg;
            
        } catch (const ResourceException& e) {
            success = false;
            errorCode = e.getErrorCode();
            errorMsg = e.getMessage();
            errorDetails = QString("Resource: %1, Type: %2")
                            .arg(e.getResource())
                            .arg(static_cast<int>(e.getType()));
            
            qCCritical(logTaskManager) << "Resource error in task:" 
                                      << task.taskId 
                                      << "Resource:" << e.getResource()
                                      << "Error:" << errorMsg;
            
        } catch (const AIServiceException& e) {
            success = false;
            errorCode = e.getErrorCode();
            errorMsg = e.getMessage();
            errorDetails = e.getDetails();
            
            qCWarning(logTaskManager) << "AI service error in task:" 
                                     << task.taskId 
                                     << "Code:" << errorCode 
                                     << "Message:" << errorMsg;
            
        } catch (const std::exception& e) {
            success = false;
            errorCode = 0; // Unknown error
            errorMsg = QString::fromStdString(e.what());
            
            qCCritical(logTaskManager) << "Unexpected error in task:" 
                                      << task.taskId 
                                      << "Error:" << errorMsg;
            
        } catch (...) {
            success = false;
            errorCode = -1; // Critical unknown error
            errorMsg = "Unknown critical error occurred";
            
            qCCritical(logTaskManager) << "Critical unknown error in task:" 
                                      << task.taskId;
        }
        
        // Async status update using single lock point
        quint64 taskId = task.taskId;
        
        QMetaObject::invokeMethod(this, [this, taskId, success, errorCode, errorMsg, errorDetails, taskResult]() {
            updateTaskState(taskId, 
                          success ? TaskStatus::Completed : TaskStatus::Failed, 
                          success, errorCode, errorMsg, errorDetails, taskResult);
        }, Qt::QueuedConnection);
        
        // Report result for responsive throttling (lockless)
        reportTaskResult(task.taskType, task.deployType, success, errorCode);
    });
    
    // Track running task with brief lock
    {
        QMutexLocker locker(&taskMutex);
        runningTasks[task.taskId] = future;
    }
}

void TaskManager::updateTaskState(quint64 taskId, TaskStatus status, bool success,
                                  int errorCode, const QString& errorMsg, const QString& errorDetails, const QVariant& taskResult)
{
    // SINGLE LOCK POINT - All task state updates happen here
    QMutexLocker locker(&taskMutex);
    
    auto taskIt = allTasks.find(taskId);
    if (taskIt == allTasks.end()) {
        qCWarning(logTaskManager) << "Cannot update state for non-existent task:" << taskId;
        return;
    }
    
    Task& task = taskIt.value();
    task.status = status;
    task.completedAt = QDateTime::currentDateTime();
    
    // Store error details if failed
    if (!success) {
        task.metadata["errorCode"] = errorCode;
        task.metadata["errorMessage"] = errorMsg;
        task.metadata["errorDetails"] = errorDetails;
    }
    
    // Update deployment-specific counters
    auto deployKey = qMakePair(task.taskType, task.deployType);
    currentTasksByDeploy[deployKey].fetchAndAddOrdered(-1);
    
    // Update metrics (no additional lock needed)
    updateTaskMetrics(task, success);
    
    // Remove from running tasks
    runningTasks.remove(taskId);
    
    // Emit appropriate signal (Qt signals are thread-safe)
    if (success) {
        emit taskCompleted(taskId, taskResult);
    } else {
        emit taskFailed(taskId, errorCode, errorMsg);
    }
    
    qCDebug(logTaskManager) << "Updated task state:" << taskId 
                           << "Status:" << taskStatusToString(status)
                           << "Success:" << success;
} 
