// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include "aidaemon_global.h"
#include "threadpool/threadpoolmanager.h"
#include "common/aitypes.h"

#include <QObject>
#include <QMutex>
#include <QTimer>
#include <QQueue>
#include <QMap>
#include <QSet>
#include <QDateTime>
#include <QVariantMap>
#include <QAtomicInt>
#include <QFuture>
#include <functional>

AIDAEMON_BEGIN_NAMESPACE

// Forward declarations
class ThreadPoolManager;

class TaskManager : public QObject
{
    Q_OBJECT
public:
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager();

    // Use unified service type definition to avoid duplication
    using TaskType = AITypes::ServiceType;

    // Task execution status
    enum class TaskStatus {
        Pending,     // Waiting for execution
        Running,     // Currently executing
        Completed,   // Successfully completed
        Failed,      // Execution failed
        Cancelled    // Cancelled by user or system
    };

    // Model deployment type for intelligent concurrency control
    enum class ModelDeployType {
        Local,      // Local models (GPU/CPU resource constrained)
        Cloud,      // Online API (network and quota constrained)
        Hybrid      // Hybrid mode
    };

    // Task structure
    struct Task {
        Task() : taskId(generateUniqueTaskId()) {}
        
        quint64 taskId;              // Auto-generated unique ID
        TaskType taskType;
        TaskStatus status = TaskStatus::Pending;
        ModelDeployType deployType = ModelDeployType::Hybrid;
        std::function<QVariant()> executor;
        QDateTime startedAt;         // When task execution started
        QDateTime completedAt;       // When task execution finished
        QString description;
        QVariantMap metadata;
        QString providerName;        // Provider name for responsive throttling
        QString modelName;           // Model name for context
        
        // Static method for generating unique task IDs
        static quint64 generateUniqueTaskId();
    };

    // Public interface methods
    void addTask(const Task& task);
    void cancelTask(quint64 taskId);
    void cancelTasksByType(TaskType type);

    void setLocalModelLimit(TaskType type, int maxTasks);

    int getCurrentConcurrencyLimit(TaskType taskType, ModelDeployType deployType) const;

    // Responsive throttling for cloud APIs
    void reportTaskResult(TaskType taskType, ModelDeployType deployType, bool success, int errorCode = 0);
    void enableResponsiveThrottling(TaskType taskType, ModelDeployType deployType, bool enabled = true);

    QList<Task> getPendingTasks(TaskType type = static_cast<TaskType>(0));
    QList<Task> getRunningTasks(TaskType type = static_cast<TaskType>(0));
    Task getTask(quint64 taskId) const;

    // Statistics methods
    int getTotalPendingCount() const;
    int getTotalRunningCount() const;
    int getTypeRunningCount(TaskType type) const;
    QMap<TaskType, int> getAllTypeCounts() const;

    // Performance monitoring (based on execution time only)
    double getAverageExecutionTime(TaskType type) const;    // startedAt -> completedAt
    double getSuccessRate(TaskType type) const;
    qint64 getTotalExecutionTime(TaskType type) const;      // Sum of all execution times

    // Static utility methods
    static QString taskTypeToString(TaskType type);
    static QString taskStatusToString(TaskStatus status);
    static TaskType stringToTaskType(const QString& typeStr);

signals:
    void taskStarted(quint64 taskId);
    void taskCompleted(quint64 taskId, const QVariant& result);
    void taskFailed(quint64 taskId, int errorCode, const QString& errorMsg);
    void taskCancelled(quint64 taskId);
    void queueSizeChanged(TaskType type, int pendingCount);

private slots:
    void dispatchTasks();
    void updateMetrics();

private:
    void handleTaskCompletion(quint64 taskId);
    void updateTaskMetrics(const Task& task, bool success);
    
    QList<Task> collectTasksToExecute();
    void executeTaskAsync(const Task& task);
    void updateTaskState(quint64 taskId, TaskStatus status, bool success = true, 
                        int errorCode = 0, const QString& errorMsg = QString(), 
                        const QString& errorDetails = QString(), const QVariant& taskResult = QVariant());

    // Task ID generation
    quint64 generateTaskId();

    // Thread pool type mapping
    ThreadPoolManager::PoolType getPoolTypeForTask(TaskType taskType) const;

    // Model deployment detection utilities
    ModelDeployType detectModelDeployType(const QString& providerName, const QString& modelName) const;
    QStringList getInstalledLocalModels() const;
    bool isLocalModelSystemAvailable() const;

    // Responsive throttling implementation
    void adjustConcurrencyBasedOnResponse(TaskType taskType, ModelDeployType deployType, bool success, int errorCode);
    bool canDispatchTask(TaskType taskType, ModelDeployType deployType) const;
    void updateThrottlingMetrics(TaskType taskType, ModelDeployType deployType);

    // Default concurrency limits (only for local models and fallback)
    // Cloud APIs use responsive throttling instead
    QMap<QPair<TaskType, ModelDeployType>, int> defaultConcurrencyLimits;

    // Task queues and tracking
    QMap<TaskType, QQueue<Task>> waitingQueues;
    QMap<quint64, Task> allTasks;
    QSet<quint64> cancelledTasks;
    QMap<quint64, QFuture<void>> runningTasks; // Track running tasks

    // Unified concurrency tracking by deployment type
    QMap<QPair<TaskType, ModelDeployType>, QAtomicInt> currentTasksByDeploy;

    // Responsive throttling state for cloud APIs
    struct ResponsiveThrottlingState
    {
        int currentConcurrency = 5;           // Current allowed concurrency
        int minConcurrency = 1;               // Minimum concurrency limit
        int maxConcurrency = 100;             // Maximum concurrency limit
        int consecutiveSuccesses = 0;         // Consecutive successful requests
        int consecutiveFailures = 0;          // Consecutive HTTP 429 failures
        QDateTime lastAdjustment;             // Last adjustment timestamp
        bool enabled = false;                 // Whether responsive throttling is enabled
        double increaseRate = 1.1;            // Concurrency increase rate (10%)
        double decreaseRate = 0.8;            // Concurrency decrease rate (20%)
        int successThreshold = 10;            // Success count needed to increase
        int cooldownMs = 5000;                // Cooldown period in milliseconds
        QQueue<QDateTime> recentRequests;     // Recent request timestamps for rate calculation
    };

    QMap<QPair<TaskType, ModelDeployType>, ResponsiveThrottlingState> throttlingStates;

    // Performance statistics
    QMap<TaskType, QList<qint64>> executionTimes;
    QMap<TaskType, QPair<int, int>> successFailCounts; // (success count, failure count)

    // Thread safety
    mutable QMutex taskMutex;
    QTimer* metricsTimer = nullptr;

    // Dispatch optimization
    QAtomicInt pendingDispatchCount;  // Track pending dispatch requests

    // Task ID generation counter

};

// Structured exception classes for robust error handling
class AIServiceException : public std::exception
{
public:
    AIServiceException(int code, const QString& message, const QString& details = QString())
        : errorCode(code), errorMessage(message), errorDetails(details)
    {
    }

    int getErrorCode() const { return errorCode; }
    QString getMessage() const { return errorMessage; }
    QString getDetails() const { return errorDetails; }

    const char* what() const noexcept override
    {
        static std::string msg;
        msg = errorMessage.toStdString();
        return msg.c_str();
    }

private:
    QString errorMessage;
    QString errorDetails;
    int errorCode = 0;
};

class NetworkException : public AIServiceException
{
public:
    NetworkException(int httpCode, const QString& message)
        : AIServiceException(httpCode, message)
    {
    }

    bool isRateLimited() const { return getErrorCode() == 429; }
    bool isServerError() const { return getErrorCode() >= 500; }
};

class ModelException : public AIServiceException
{
public:
    enum class Type {
        LoadFailure = 1001,
        InferenceFailure = 1002,
        ResourceExhausted = 1003,
        InvalidInput = 1004
    };

    ModelException(Type type, const QString& modelName, const QString& details = QString())
        : AIServiceException(static_cast<int>(type),
                           QString("Model operation failed: %1").arg(modelName), details)
        , type(type)
        , modelName(modelName)
    {
    }

    Type getType() const { return type; }
    QString getModelName() const { return modelName; }

private:
    Type type;
    QString modelName;
};

class ResourceException : public AIServiceException
{
public:
    enum class Type {
        InsufficientMemory = 2001,
        InsufficientDisk = 2002,
        FileNotFound = 2003,
        PermissionDenied = 2004
    };

    ResourceException(Type type, const QString& resource, const QString& details = QString())
        : AIServiceException(static_cast<int>(type),
                           QString("Resource error: %1").arg(resource), details)
        , type(type)
        , resource(resource)
    {
    }

    Type getType() const { return type; }
    QString getResource() const { return resource; }

private:
    Type type;
    QString resource;
};

AIDAEMON_END_NAMESPACE

#endif // TASKMANAGER_H
