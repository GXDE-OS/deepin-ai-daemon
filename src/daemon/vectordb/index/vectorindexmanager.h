// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORINDEXMANAGER_H
#define VECTORINDEXMANAGER_H

#include "aidaemon_global.h"
#include "faissindexwrapper.h"

#include <QObject>
#include <QHash>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QMutex>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <QVector>
#include <QBitArray>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class IndexCache;

/**
 * @brief Central coordinator for vector indexing operations
 * 
 * Handles vector generation, index construction, deletion operations,
 * and search functionality with support for multiple indexing strategies.
 */
class VectorIndexManager : public QObject
{
    Q_OBJECT

public:
    // Use FaissIndexWrapper's enums to avoid duplication
    using IndexType = FaissIndexWrapper::IndexType;
    using DistanceMetric = FaissIndexWrapper::DistanceMetric;

    explicit VectorIndexManager(const QString &workDir);
    ~VectorIndexManager();

    /**
     * @brief Create a new vector index
     * @param config Index configuration
     * @return indexid
     */
    QString createIndex(const QVariantMap &config);

    /**
     * @brief Delete an existing index
     * @param indexId Index identifier
     * @return true on success, false on failure
     */
    bool deleteIndex(const QString &indexId);

    /**
     * @brief Rebuild an existing index
     * @param indexId Index identifier
     * @param vectors List of vectors to rebuild with
     * @return true on success, false on failure
     */
    bool rebuildIndex(const QString &indexId, const QVariantList &vectors);

    /**
     * @brief Add vectors to an index
     * @param indexId Index identifier
     * @param vectors List of vectors to add
     * @return true on success, false on failure
     */
    bool addVectors(const QString &indexId, const QVariantList &vectors);

    /**
     * @brief Remove vectors from an index
     * @param indexId Index identifier
     * @param vectorIds List of vector IDs to remove
     * @return true on success, false on failure
     */
    bool removeVectors(const QString &indexId, const QStringList &vectorIds);

    /**
     * @brief Update a vector in an index
     * @param indexId Index identifier
     * @param vectorId Vector identifier
     * @param vector New vector data
     * @return true on success, false on failure
     */
    bool updateVector(const QString &indexId, const QString &vectorId, const QVector<float> &vector);

    /**
     * @brief Search for similar vectors
     * @param queryVector Query vector
     * @param topK Number of results to return
     * @param searchParams Search parameters
     * @return Search results as QVariantList
     */
    QVariantList search(const QVector<float> &queryVector, int topK = 10, const QVariantMap &searchParams = QVariantMap());

    /**
     * @brief Batch search for multiple query vectors
     * @param indexId Index identifier
     * @param queryVectors List of query vectors
     * @param topK Number of results per query
     * @return Batch search results as QVariantList
     */
    QVariantList batchSearch(const QString &indexId, const QVariantList &queryVectors, int topK = 10);

    /**
     * @brief Range search within a radius
     * @param indexId Index identifier
     * @param queryVector Query vector
     * @param radius Search radius
     * @return Search results within radius
     */
    QVariantList rangeSearch(const QString &indexId, const QVector<float> &queryVector, float radius);

    /**
     * @brief Get index information
     * @param indexId Index identifier
     * @return Index information as QVariantMap
     */
    QVariantMap getIndexInfo(const QString &indexId) const;

    /**
     * @brief Get index statistics
     * @param indexId Index identifier
     * @return Index statistics as QVariantMap
     */
    QVariantMap getIndexStatistics(const QString &indexId) const;

    /**
     * @brief Check if index is ready for operations
     * @param indexId Index identifier
     * @return true if ready, false otherwise
     */
    bool isIndexReady(const QString &indexId) const;

    /**
     * @brief Get index size in bytes
     * @param indexId Index identifier
     * @return Index size in bytes
     */
    qint64 getIndexSize(const QString &indexId) const;

    /**
     * @brief Optimize index for better performance
     * @param indexId Index identifier
     * @return true on success, false on failure
     */
    bool optimizeIndex(const QString &indexId);

    /**
     * @brief Compact index to reduce size
     * @param indexId Index identifier
     * @return true on success, false on failure
     */
    bool compactIndex(const QString &indexId);

    /**
     * @brief Validate index consistency
     * @param indexId Index identifier
     * @return true if consistent, false otherwise
     */
    bool validateIndexConsistency(const QString &indexId);

    /**
     * @brief Set cache size for index caching
     * @param sizeInBytes Cache size in bytes
     */
    void setCacheSize(qint64 sizeInBytes);

    /**
     * @brief Clear all cached indexes
     */
    void clearCache();

    /**
     * @brief Get cache statistics
     * @return Cache statistics as QVariantMap
     */
    QVariantMap getCacheStatistics() const;

    bool saveIndexToFile(const QString &indexId);

public Q_SLOTS:
    /**
     * @brief Handle memory pressure
     */
    void onMemoryPressure();

    /**
     * @brief Handle index optimization completion
     * @param indexId Index identifier
     */
    void onIndexOptimizationCompleted(const QString &indexId);

Q_SIGNALS:
    /**
     * @brief Emitted when an index is created
     * @param indexId Index identifier
     */
    void indexCreated(const QString &indexId);

    /**
     * @brief Emitted when an index is deleted
     * @param indexId Index identifier
     */
    void indexDeleted(const QString &indexId);

    /**
     * @brief Emitted when vectors are added to an index
     * @param indexId Index identifier
     * @param count Number of vectors added
     */
    void vectorsAdded(const QString &indexId, int count);

    /**
     * @brief Emitted when vectors are removed from an index
     * @param indexId Index identifier
     * @param count Number of vectors removed
     */
    void vectorsRemoved(const QString &indexId, int count);

    /**
     * @brief Emitted when an index is optimized
     * @param indexId Index identifier
     */
    void indexOptimized(const QString &indexId);

    /**
     * @brief Emitted when search is completed
     * @param indexId Index identifier
     * @param results Search results
     */
    void searchCompleted(const QVariantList &results);

    /**
     * @brief Emitted when an error occurs
     * @param operation Operation that failed
     * @param error Error message
     */
    void errorOccurred(const QString &operation, const QString &error);

private:
    QString m_indexDir;
    void initIndexDir(const QString &workDir);
    
    // FAISS index management
    QHash<QString, QSharedPointer<FaissIndexWrapper>> m_indexes;
    QMutex m_indexMutex;
    
    // Caching system
    qint64 m_maxCacheSize = 1024 * 1024 * 1024; // 1GB default
    QScopedPointer<IndexCache> m_indexCache;
    
    // Index selection and optimization
    IndexType selectOptimalIndexType(int vectorCount, int dimension, const QVariantMap &requirements) const;
    QVariantMap getOptimalIndexParams(IndexType type, int vectorCount, int dimension) const;
    
    // File management
    QString getIndexFilePath(const QString &indexId) const;
    bool loadIndexFromFile(const QString &indexId);
    
    // Consistency and validation
    bool verifyIndexConsistency(const QString &indexId) const;
    
    // Memory and resource management
    void evictLeastRecentlyUsedIndex();
    qint64 calculateIndexMemoryUsage(const QString &indexId) const;
    
    // Helper methods
    bool validateIndexId(const QString &indexId) const;
    bool validateVectorId(const QString &vectorId) const;
    bool validateVector(const QVector<float> &vector) const;
    bool validateConfig(const QVariantMap &config) const;
    QString generateIndexId() const;
    QVariantList searchInIndex(const QString &indexId, const QVector<float> &queryVector, int topK = 10, const QVariantMap &searchParams = QVariantMap());
    QStringList getAllIndexIds() const;
    
    // Index creation helpers
    QSharedPointer<FaissIndexWrapper> createFaissIndex(IndexType type, int dimension, DistanceMetric metric);
    bool initializeIndex(const QString &indexId, const QVariantMap &config);
    void cleanupIndex(const QString &indexId);
    QSharedPointer<FaissIndexWrapper> getIndexFromCache(const QString &indexId) const;
    QSharedPointer<FaissIndexWrapper> getIndex(const QString &indexId);
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORINDEXMANAGER_H
