// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXCACHE_H
#define INDEXCACHE_H

#include "aidaemon_global.h"

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSharedPointer>
#include <QtCore/QMutex>
#include <QtCore/QDateTime>
#include <QtCore/QTimer>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class FaissIndexWrapper;

/**
 * @brief LRU cache for vector indexes
 * 
 * Manages in-memory caching of vector indexes with LRU eviction
 * policy to optimize memory usage and access performance.
 */
class IndexCache : public QObject
{
    Q_OBJECT

public:
    struct CacheEntry {
        QSharedPointer<FaissIndexWrapper> index;
        QDateTime lastAccessed;
        qint64 memoryUsage;
        int accessCount;
    };

    explicit IndexCache(qint64 maxSizeBytes = 1024 * 1024 * 1024, QObject *parent = nullptr); // 1GB default
    ~IndexCache();

    /**
     * @brief Set maximum cache size
     * @param sizeBytes Maximum size in bytes
     */
    void setMaxSize(qint64 sizeBytes);

    /**
     * @brief Get maximum cache size
     * @return Maximum size in bytes
     */
    qint64 getMaxSize() const { return m_maxSize; }

    /**
     * @brief Get current cache size
     * @return Current size in bytes
     */
    qint64 getCurrentSize() const;

    /**
     * @brief Add index to cache
     * @param indexId Index identifier
     * @param index Index wrapper to cache
     * @return true if added, false if failed
     */
    bool addIndex(const QString &indexId, QSharedPointer<FaissIndexWrapper> index);

    /**
     * @brief Get index from cache
     * @param indexId Index identifier
     * @return Index wrapper, or nullptr if not found
     */
    QSharedPointer<FaissIndexWrapper> getIndex(const QString &indexId);

    /**
     * @brief Remove index from cache
     * @param indexId Index identifier
     * @return true if removed, false if not found
     */
    bool removeIndex(const QString &indexId);

    /**
     * @brief Check if index is cached
     * @param indexId Index identifier
     * @return true if cached, false otherwise
     */
    bool hasIndex(const QString &indexId) const;

    /**
     * @brief Clear all cached indexes
     */
    void clear();

    /**
     * @brief Get cache statistics
     * @return Statistics as QVariantMap
     */
    QVariantMap getStatistics() const;

    /**
     * @brief Get cache hit rate
     * @return Hit rate as percentage (0.0 - 1.0)
     */
    double getHitRate() const;

    /**
     * @brief Get number of cached indexes
     * @return Number of indexes in cache
     */
    int getIndexCount() const;

    /**
     * @brief Get list of cached index IDs
     * @return List of index IDs
     */
    QStringList getCachedIndexIds() const;

public Q_SLOTS:
    /**
     * @brief Evict least recently used indexes to free memory
     */
    void evictLeastRecentlyUsed();

    /**
     * @brief Perform cache maintenance
     */
    void performMaintenance();

Q_SIGNALS:
    /**
     * @brief Emitted when an index is evicted from cache
     * @param indexId Index identifier
     * @param reason Eviction reason
     */
    void indexEvicted(const QString &indexId, const QString &reason);

    /**
     * @brief Emitted when cache size limit is reached
     * @param currentSize Current cache size
     * @param maxSize Maximum cache size
     */
    void cacheSizeLimitReached(qint64 currentSize, qint64 maxSize);

private:
    mutable QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
    qint64 m_maxSize;
    
    // Statistics
    mutable qint64 m_totalRequests = 0;
    mutable qint64 m_cacheHits = 0;
    
    // Maintenance timer
    QTimer *m_maintenanceTimer;
    
    // Helper methods
    void updateAccessInfo(const QString &indexId);
    QString findLeastRecentlyUsedIndex() const;
    bool needsEviction() const;
    void evictIndex(const QString &indexId, const QString &reason);
    qint64 calculateIndexMemoryUsage(QSharedPointer<FaissIndexWrapper> index) const;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // INDEXCACHE_H
