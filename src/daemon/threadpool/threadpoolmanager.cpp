// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "threadpoolmanager.h"

#include <QThread>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <mutex>

Q_DECLARE_LOGGING_CATEGORY(logTaskManager)

AIDAEMON_USE_NAMESPACE

// Default thread pool configuration
const QMap<ThreadPoolManager::PoolType, int> ThreadPoolManager::defaultMaxThreads = {
    {PoolType::General, QThread::idealThreadCount()},      // General: CPU core count
    {PoolType::IOBound, QThread::idealThreadCount() * 2},  // IO-intensive: 2x CPU core count
    {PoolType::CPUBound, QThread::idealThreadCount()},     // CPU-intensive: CPU core count
    {PoolType::Critical, 4}                                // Critical tasks: fixed 4 threads
};

ThreadPoolManager& ThreadPoolManager::getInstance()
{
    static ThreadPoolManager instance;
    static bool initialized = false;
    if (!initialized) {
        instance.initializePools();
        initialized = true;
    }
    return instance;
}

void ThreadPoolManager::initializePools()
{
    QMutexLocker locker(&poolMutex);
    
    // Create thread pools for each type
    for (auto it = defaultMaxThreads.begin(); it != defaultMaxThreads.end(); ++it) {
        PoolType type = it.key();
        int maxThreads = it.value();
        
        QThreadPool* pool = new QThreadPool();
        pool->setMaxThreadCount(maxThreads);
        
        // Note: Thread priority is set at the task level in QtConcurrent::run,
        // not at the pool level. QThreadPool doesn't support setThreadPriority.
        
        threadPools[type] = pool;
        qCInfo(logTaskManager) << "Initialized thread pool:" << static_cast<int>(type)
                              << "with max threads:" << maxThreads;
    }
    
    // Set up monitoring timer
    monitorTimer = new QTimer(this);
    connect(monitorTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker locker(&poolMutex);
        for (auto it = threadPools.begin(); it != threadPools.end(); ++it) {
            PoolType type = it.key();
            QThreadPool* pool = it.value();
            emit poolStatusChanged(type, pool->activeThreadCount(), pool->maxThreadCount());
        }
    });
    monitorTimer->start(30000); // Monitor every 30 seconds
}

QThreadPool* ThreadPoolManager::getThreadPool(PoolType type)
{
    QMutexLocker locker(&poolMutex);
    auto it = threadPools.find(type);
    if (it != threadPools.end()) {
        return it.value();
    }
    
    qCWarning(logTaskManager) << "Thread pool not found for type:" << static_cast<int>(type);
    return threadPools.value(PoolType::General, nullptr);
}

void ThreadPoolManager::configurePool(PoolType type, int maxThreadCount)
{
    QMutexLocker locker(&poolMutex);
    auto it = threadPools.find(type);
    if (it != threadPools.end()) {
        it.value()->setMaxThreadCount(maxThreadCount);
        qCInfo(logTaskManager) << "Configured thread pool:" << static_cast<int>(type)
                              << "max threads:" << maxThreadCount;
    } else {
        qCWarning(logTaskManager) << "Cannot configure non-existent thread pool:" << static_cast<int>(type);
    }
}

int ThreadPoolManager::getActiveThreadCount(PoolType type) const
{
    QMutexLocker locker(&poolMutex);
    auto it = threadPools.find(type);
    return it != threadPools.end() ? it.value()->activeThreadCount() : 0;
}

int ThreadPoolManager::getMaxThreadCount(PoolType type) const
{
    QMutexLocker locker(&poolMutex);
    auto it = threadPools.find(type);
    return it != threadPools.end() ? it.value()->maxThreadCount() : 0;
}

bool ThreadPoolManager::isPoolActive(PoolType type) const
{
    QMutexLocker locker(&poolMutex);
    auto it = threadPools.find(type);
    return it != threadPools.end() && it.value()->activeThreadCount() > 0;
} 
