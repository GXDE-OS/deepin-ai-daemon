// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexcache.h"
#include "faissindexwrapper.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutexLocker>
#include <QtCore/QVariantMap>
#include <algorithm>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(indexCache, "aidaemon.vectordb.indexcache")

IndexCache::IndexCache(qint64 maxSizeBytes, QObject *parent)
    : QObject(parent)
    , m_maxSize(maxSizeBytes)
    , m_maintenanceTimer(new QTimer(this))
{
    qCDebug(indexCache) << "Creating index cache with max size:" << maxSizeBytes << "bytes";
    
    // Set up maintenance timer (run every 5 minutes)
    m_maintenanceTimer->setInterval(5 * 60 * 1000);
    m_maintenanceTimer->setSingleShot(false);
    connect(m_maintenanceTimer, &QTimer::timeout, this, &IndexCache::performMaintenance);
    m_maintenanceTimer->start();
}

IndexCache::~IndexCache()
{
    qCDebug(indexCache) << "Destroying index cache with" << getIndexCount() << "cached indexes";
    clear();
}

void IndexCache::setMaxSize(qint64 sizeBytes)
{
    QMutexLocker locker(&m_mutex);
    
    qCDebug(indexCache) << "Setting max cache size to" << sizeBytes << "bytes";
    m_maxSize = sizeBytes;
    
    // Trigger eviction if current size exceeds new limit
    if (getCurrentSize() > m_maxSize) {
        locker.unlock();
        evictLeastRecentlyUsed();
    }
}

qint64 IndexCache::getCurrentSize() const
{
    QMutexLocker locker(&m_mutex);
    
    qint64 totalSize = 0;
    for (const CacheEntry &entry : m_cache) {
        totalSize += entry.memoryUsage;
    }
    
    return totalSize;
}

bool IndexCache::addIndex(const QString &indexId, QSharedPointer<FaissIndexWrapper> index)
{
    if (indexId.isEmpty() || !index) {
        qCWarning(indexCache) << "Cannot add invalid index to cache";
        return false;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        // Check if index already exists
        if (m_cache.contains(indexId)) {
            qCDebug(indexCache) << "Index already cached, updating:" << indexId;
            updateAccessInfo(indexId);
            return true;
        }
    }
    
    // Calculate memory usage
    qint64 memoryUsage = calculateIndexMemoryUsage(index);
    
    // Check if adding this index would exceed cache limit
    if (getCurrentSize() + memoryUsage > m_maxSize) {
        evictLeastRecentlyUsed();
        
        // Check again after eviction
        if (getCurrentSize() + memoryUsage > m_maxSize) {
            qCWarning(indexCache) << "Cannot add index to cache: would exceed size limit";
            emit cacheSizeLimitReached(getCurrentSize() + memoryUsage, m_maxSize);
            return false;
        }
    }
    
    // Add to cache
    CacheEntry entry;
    entry.index = index;
    entry.lastAccessed = QDateTime::currentDateTime();
    entry.memoryUsage = memoryUsage;
    entry.accessCount = 1;
    
    m_cache[indexId] = entry;
    
    qCDebug(indexCache) << "Added index to cache:" << indexId 
                        << "size:" << memoryUsage << "bytes"
                        << "total cached:" << getIndexCount();
    
    return true;
}

QSharedPointer<FaissIndexWrapper> IndexCache::getIndex(const QString &indexId)
{
    QSharedPointer<FaissIndexWrapper> result;
    bool found = false;
    
    {
        QMutexLocker locker(&m_mutex);
        
        m_totalRequests++;
        
        if (m_cache.contains(indexId)) {
            m_cacheHits++;
            updateAccessInfo(indexId);
            result = m_cache[indexId].index;
            found = true;
        }
    }
    
    if (found) {
        qCDebug(indexCache) << "Cache hit for index:" << indexId;
        return result;
    } else {
        qCDebug(indexCache) << "Cache miss for index:" << indexId;
        return nullptr;
    }
}

bool IndexCache::removeIndex(const QString &indexId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_cache.contains(indexId)) {
        qCDebug(indexCache) << "Index not found in cache for removal:" << indexId;
        return false;
    }
    
    qint64 memoryUsage = m_cache[indexId].memoryUsage;
    m_cache.remove(indexId);
    
    qCDebug(indexCache) << "Removed index from cache:" << indexId 
                        << "freed:" << memoryUsage << "bytes"
                        << "remaining:" << getIndexCount();
    
    return true;
}

bool IndexCache::hasIndex(const QString &indexId) const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.contains(indexId);
}

void IndexCache::clear()
{
    int count = m_cache.size();
    qint64 totalSize = getCurrentSize();
    
    m_cache.clear();
    
    qCDebug(indexCache) << "Cleared cache:" << count << "indexes," << totalSize << "bytes freed";
}

QVariantMap IndexCache::getStatistics() const
{
    QVariantMap stats;
    stats["maxSize"] = m_maxSize;
    stats["currentSize"] = getCurrentSize();
    stats["indexCount"] = getIndexCount();
    stats["hitRate"] = getHitRate();
    stats["totalRequests"] = static_cast<qint64>(m_totalRequests);
    stats["cacheHits"] = static_cast<qint64>(m_cacheHits);
    stats["cacheMisses"] = static_cast<qint64>(m_totalRequests - m_cacheHits);
    
    return stats;
}

double IndexCache::getHitRate() const
{
    QMutexLocker locker(&m_mutex);
    
    if (m_totalRequests == 0) {
        return 0.0;
    }
    
    return static_cast<double>(m_cacheHits) / static_cast<double>(m_totalRequests);
}

int IndexCache::getIndexCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.size();
}

QStringList IndexCache::getCachedIndexIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.keys();
}

void IndexCache::evictLeastRecentlyUsed()
{
    QMutexLocker locker(&m_mutex);
    
    while (needsEviction() && !m_cache.isEmpty()) {
        QString lruIndexId = findLeastRecentlyUsedIndex();
        if (!lruIndexId.isEmpty()) {
            evictIndex(lruIndexId, "LRU eviction");
        } else {
            break; // Safety check
        }
    }
}

void IndexCache::performMaintenance()
{
    qCDebug(indexCache) << "Performing cache maintenance";
    
    // Check if eviction is needed
    if (needsEviction()) {
        evictLeastRecentlyUsed();
    }
    
    // Log cache statistics
    QVariantMap stats = getStatistics();
    qCDebug(indexCache) << "Cache stats:" 
                        << "size:" << stats["currentSize"].toLongLong() << "/" << stats["maxSize"].toLongLong()
                        << "indexes:" << stats["indexCount"].toInt()
                        << "hit rate:" << QString::number(stats["hitRate"].toDouble() * 100, 'f', 1) << "%";
}

void IndexCache::updateAccessInfo(const QString &indexId)
{
    if (m_cache.contains(indexId)) {
        CacheEntry &entry = m_cache[indexId];
        entry.lastAccessed = QDateTime::currentDateTime();
        entry.accessCount++;
    }
}

QString IndexCache::findLeastRecentlyUsedIndex() const
{
    if (m_cache.isEmpty()) {
        return QString();
    }
    
    QString lruIndexId;
    QDateTime oldestAccess = QDateTime::currentDateTime();
    
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        if (it.value().lastAccessed < oldestAccess) {
            oldestAccess = it.value().lastAccessed;
            lruIndexId = it.key();
        }
    }
    
    return lruIndexId;
}

bool IndexCache::needsEviction() const
{
    return getCurrentSize() > m_maxSize;
}

void IndexCache::evictIndex(const QString &indexId, const QString &reason)
{
    if (m_cache.contains(indexId)) {
        qint64 memoryUsage = m_cache[indexId].memoryUsage;
        m_cache.remove(indexId);
        
        qCDebug(indexCache) << "Evicted index from cache:" << indexId 
                            << "reason:" << reason
                            << "freed:" << memoryUsage << "bytes";
        
        emit indexEvicted(indexId, reason);
    }
}

qint64 IndexCache::calculateIndexMemoryUsage(QSharedPointer<FaissIndexWrapper> index) const
{
    if (!index) {
        return 0;
    }
    
    return index->getMemoryUsage();
}

AIDAEMON_VECTORDB_END_NAMESPACE
