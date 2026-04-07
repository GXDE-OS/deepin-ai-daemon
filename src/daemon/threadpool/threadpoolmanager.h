// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THREADPOOLMANAGER_H
#define THREADPOOLMANAGER_H

#include "aidaemon_global.h"

#include <QObject>
#include <QThreadPool>
#include <QMutex>
#include <QTimer>
#include <QMap>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

AIDAEMON_BEGIN_NAMESPACE

class ThreadPoolManager : public QObject
{
    Q_OBJECT
public:
    static ThreadPoolManager& getInstance();
    
    // Configure specialized thread pools for different types of tasks
    enum class PoolType
    {
        General,        // General purpose task pool
        IOBound,        // IO-intensive task pool  
        CPUBound,       // CPU-intensive task pool
        Critical        // Critical task pool (high priority)
    };
    
    QThreadPool* getThreadPool(PoolType type);
    void configurePool(PoolType type, int maxThreadCount);
    
    // Convenient task submission interface
    template<typename Func, typename... Args>
    QFuture<void> submitTask(PoolType poolType, Func&& func, Args&&... args)
    {
        auto pool = getThreadPool(poolType);
        return QtConcurrent::run(pool, std::forward<Func>(func), std::forward<Args>(args)...);
    }
    
    template<typename Func, typename... Args>
    QFuture<typename std::invoke_result_t<Func, Args...>> 
    submitTaskWithResult(PoolType poolType, Func&& func, Args&&... args)
    {
        auto pool = getThreadPool(poolType);
        return QtConcurrent::run(pool, std::forward<Func>(func), std::forward<Args>(args)...);
    }
    
    // Monitoring interface
    int getActiveThreadCount(PoolType type) const;
    int getMaxThreadCount(PoolType type) const;
    bool isPoolActive(PoolType type) const;
    
signals:
    void poolStatusChanged(PoolType type, int activeThreads, int maxThreads);
    
private:
    ThreadPoolManager() = default;
    ~ThreadPoolManager() = default;
    ThreadPoolManager(const ThreadPoolManager&) = delete;
    ThreadPoolManager& operator=(const ThreadPoolManager&) = delete;
    
    void initializePools();
    
    QMap<PoolType, QThreadPool*> threadPools;
    mutable QMutex poolMutex;
    QTimer* monitorTimer = nullptr;
    
    // Default thread pool configuration
    static const QMap<PoolType, int> defaultMaxThreads;
};

AIDAEMON_END_NAMESPACE

#endif // THREADPOOLMANAGER_H
