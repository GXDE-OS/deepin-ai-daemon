// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vectorindexmanager.h"
#include "faissindexwrapper.h"
#include "vectoridmapper.h"
#include "indexcache.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QUuid>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(vectorIndexManager, "vectordb.index.manager")

VectorIndexManager::VectorIndexManager(const QString &workDir)
    : QObject(nullptr)
    , m_indexCache(new IndexCache(m_maxCacheSize, this))
{
    qCDebug(vectorIndexManager) << "VectorIndexManager created for project:" << workDir;
    
    initIndexDir(workDir);

    // Connect cache signals
    connect(m_indexCache.data(), &IndexCache::indexEvicted,
            this, [](const QString &indexId, const QString &reason) {
                qCDebug(vectorIndexManager) << "Index evicted from cache:" << indexId << "reason:" << reason;
            });
}

VectorIndexManager::~VectorIndexManager()
{
    qCDebug(vectorIndexManager) << "VectorIndexManager destroyed";
    
    // Clean up all indexes
    QMutexLocker locker(&m_indexMutex);
    for (auto it = m_indexes.begin(); it != m_indexes.end(); ++it) {
        cleanupIndex(it.key());
    }
}

QString VectorIndexManager::createIndex(const QVariantMap &config)
{
    qCDebug(vectorIndexManager) << "Creating index...";
    
    if (!validateConfig(config)) {
        qCWarning(vectorIndexManager) << "Invalid parameters for createIndex";
        return QString();
    }
    
    bool initSuccess = false;

    QString indexId = "index_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    initSuccess = initializeIndex(indexId, config);
    
    if (!initSuccess) {
        qCWarning(vectorIndexManager) << "Failed to initialize index:" << indexId;
        return QString();
    }
    
    emit indexCreated(indexId);
    qCDebug(vectorIndexManager) << "Index created successfully:" << indexId;
    return indexId;
}

bool VectorIndexManager::deleteIndex(const QString &indexId)
{
    qCDebug(vectorIndexManager) << "Deleting index:" << indexId;
    
    if (!validateIndexId(indexId)) {
        qCWarning(vectorIndexManager) << "Invalid index ID:" << indexId;
        return false;
    }
    
    bool indexExists = false;
    
    {
        QMutexLocker locker(&m_indexMutex);
        
        if (!m_indexes.contains(indexId)) {
            // Index doesn't exist
        } else {
            indexExists = true;
            
            // Clean up index
            cleanupIndex(indexId);
            
            // Remove from tracking
            m_indexes.remove(indexId);
        }
    }
    
    if (!indexExists) {
        qCWarning(vectorIndexManager) << "Index does not exist:" << indexId;
        return false;
    }
    
    emit indexDeleted(indexId);
    qCDebug(vectorIndexManager) << "Index deleted successfully:" << indexId;
    return true;
}

bool VectorIndexManager::rebuildIndex(const QString &indexId, const QVariantList &vectors)
{
    qCDebug(vectorIndexManager) << "Rebuilding index:" << indexId << "with" << vectors.size() << "vectors";
    
    if (!validateIndexId(indexId)) {
        qCWarning(vectorIndexManager) << "Invalid index ID:" << indexId;
        return false;
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for rebuilding:" << indexId;
        return false;
    }
    
    // Reset the index
    index->reset();
    
    // Re-add all vectors
    return index->addVectors(vectors);
}

bool VectorIndexManager::addVectors(const QString &indexId, const QVariantList &vectors)
{
    qCDebug(vectorIndexManager) << "Adding" << vectors.size() << "vectors to index:" << indexId;
    
    if (!validateIndexId(indexId) || vectors.isEmpty()) {
        qCWarning(vectorIndexManager) << "Invalid parameters for addVectors";
        return false;
    }
    
    QMutexLocker locker(&m_indexMutex);
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for adding vectors:" << indexId;
        return false;
    }
    
    // Add vectors to the index
    bool success = index->addVectors(vectors);
    if (success) {
        saveIndexToFile(indexId);

        emit vectorsAdded(indexId, vectors.size());
    }
    
    return success;
}

bool VectorIndexManager::removeVectors(const QString &indexId, const QStringList &vectorIds)
{
    qCDebug(vectorIndexManager) << "Removing" << vectorIds.size() << "vectors from index:" << indexId;
    
    if (!validateIndexId(indexId) || vectorIds.isEmpty()) {
        qCWarning(vectorIndexManager) << "Invalid parameters for removeVectors";
        return false;
    }
    
    QMutexLocker locker(&m_indexMutex);
    // Get the index from cache or storage
    QSharedPointer<FaissIndexWrapper> index = getIndex(indexId);
    if (!index)
        return false;
    
    // Remove vectors from the index
    bool success = index->removeVectors(vectorIds);
    if (success) {
        saveIndexToFile(indexId);
        emit vectorsRemoved(indexId, vectorIds.size());
    }
    
    return success;
}

bool VectorIndexManager::updateVector(const QString &indexId, const QString &vectorId, const QVector<float> &vector)
{
    qCDebug(vectorIndexManager) << "Updating vector:" << vectorId << "in index:" << indexId;
    
    if (!validateIndexId(indexId) || !validateVectorId(vectorId) || !validateVector(vector)) {
        qCWarning(vectorIndexManager) << "Invalid parameters for updateVector";
        return false;
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for updating vector:" << indexId;
        return false;
    }
    
    // Update vector by removing old and adding new
    QStringList vectorIds = {vectorId};
    bool removeSuccess = index->removeVectors(vectorIds);
    
    if (removeSuccess) {
        // Create vector data for adding
        QVariantMap vectorData;
        vectorData["id"] = vectorId;
        
        QVariantList vectorList;
        for (float value : vector) {
            vectorList.append(value);
        }
        vectorData["vector"] = vectorList;
        
        QVariantList vectors = {vectorData};
        return index->addVectors(vectors);
    }
    
    return false;
}

QVariantList VectorIndexManager::search(const QVector<float> &queryVector, int topK, const QVariantMap &searchParams)
{
    qCDebug(vectorIndexManager) << "Searching across all indexes...";
    
    if (!validateVector(queryVector) || topK <= 0) {
        qCWarning(vectorIndexManager) << "Invalid parameters for search";
        return QVariantList();
    }

    // 收集所有索引的搜索结果
    QVariantList allResults;
    QStringList allIndexIds = getAllIndexIds();
    
    for (const QString &indexId : allIndexIds) {
        QVariantList indexResults = searchInIndex(indexId, queryVector, topK, searchParams);
        allResults.append(indexResults);
    }
    
    // 如果没有任何结果，直接返回
    if (allResults.isEmpty()) {
        emit searchCompleted(QVariantList());
        return QVariantList();
    }
    
    // 合并并排序所有结果
    std::sort(allResults.begin(), allResults.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap mapA = a.toMap();
        const QVariantMap mapB = b.toMap();
        return mapA.value("similarity").toFloat() > mapB.value("similarity").toFloat();
    });

    // 只保留topK个最佳结果
    QVariantList finalResults;
    int resultCount = qMin(allResults.size(), topK);
    for (int i = 0; i < resultCount; ++i) {
        finalResults.append(allResults.at(i));
    }

    emit searchCompleted(finalResults);
    return finalResults;
}

QVariantList VectorIndexManager::batchSearch(const QString &indexId, const QVariantList &queryVectors, int topK)
{
    qCDebug(vectorIndexManager) << "Batch searching in index:" << indexId << "queries:" << queryVectors.size() << "topK:" << topK;
    
    if (!validateIndexId(indexId) || queryVectors.isEmpty() || topK <= 0) {
        qCWarning(vectorIndexManager) << "Invalid parameters for batchSearch";
        return QVariantList();
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for batch search:" << indexId;
        return QVariantList();
    }
    
    // Convert QVariantList to QList<QVector<float>>
    QList<QVector<float>> queryVectorList;
    for (const QVariant &queryData : queryVectors) {
        QVariantList vectorList = queryData.toList();
        QVector<float> vector;
        vector.reserve(vectorList.size());
        for (const QVariant &value : vectorList) {
            vector.append(value.toFloat());
        }
        queryVectorList.append(vector);
    }
    
    // Perform batch search
    QList<QList<FaissIndexWrapper::SearchResult>> batchResults = index->batchSearch(queryVectorList, topK);
    
    // Convert to QVariantList
    QVariantList results;
    for (const auto &searchResults : batchResults) {
        QVariantList queryResults;
        for (const auto &result : searchResults) {
            QVariantMap resultMap;
            resultMap["vectorId"] = result.vectorId;
            resultMap["similarity"] = result.similarity;
            resultMap["metadata"] = result.metadata;
            queryResults.append(resultMap);
        }
        results.append(queryResults);
    }
    
    return results;
}

QVariantList VectorIndexManager::rangeSearch(const QString &indexId, const QVector<float> &queryVector, float radius)
{
    qCDebug(vectorIndexManager) << "Range searching in index:" << indexId << "radius:" << radius;
    
    if (!validateIndexId(indexId) || !validateVector(queryVector) || radius <= 0) {
        qCWarning(vectorIndexManager) << "Invalid parameters for rangeSearch";
        return QVariantList();
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for range search:" << indexId;
        return QVariantList();
    }
    
    // Perform range search
    QList<FaissIndexWrapper::SearchResult> searchResults = index->rangeSearch(queryVector, radius);
    
    // Convert to QVariantList
    QVariantList results;
    for (const auto &result : searchResults) {
        QVariantMap resultMap;
        resultMap["vectorId"] = result.vectorId;
        resultMap["similarity"] = result.similarity;
        resultMap["metadata"] = result.metadata;
        results.append(resultMap);
    }
    
    return results;
}

QVariantMap VectorIndexManager::getIndexInfo(const QString &indexId) const
{
    if (!validateIndexId(indexId)) {
        return QVariantMap();
    }
    
    QMutexLocker locker(&const_cast<QMutex&>(m_indexMutex));
    
    QVariantMap info;
    info["indexId"] = indexId;
    info["indexDir"] = m_indexDir;
    info["isReady"] = isIndexReady(indexId);
    
    return info;
}

QVariantMap VectorIndexManager::getIndexStatistics(const QString &indexId) const
{
    if (!validateIndexId(indexId)) {
        return QVariantMap();
    }
    
    QVariantMap stats;
    stats["indexId"] = indexId;
    // Get actual vector count from index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    stats["vectorCount"] = index ? index->getVectorCount() : 0;
    stats["indexSize"] = getIndexSize(indexId);
    stats["memoryUsage"] = calculateIndexMemoryUsage(indexId);
    
    return stats;
}

bool VectorIndexManager::isIndexReady(const QString &indexId) const
{
    return m_indexes.contains(indexId) && m_indexes[indexId];
}

qint64 VectorIndexManager::getIndexSize(const QString &indexId) const
{
    QString indexPath = getIndexFilePath(indexId);
    QFileInfo fileInfo(indexPath);
    
    if (fileInfo.exists()) {
        return fileInfo.size();
    }
    
    return 0;
}

bool VectorIndexManager::optimizeIndex(const QString &indexId)
{
    qCDebug(vectorIndexManager) << "Optimizing index:" << indexId;
    
    if (!validateIndexId(indexId)) {
        qCWarning(vectorIndexManager) << "Invalid index ID:" << indexId;
        return false;
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for optimization:" << indexId;
        return false;
    }
    
    // Optimize the index
    bool success = index->optimize();
    if (success) {
        emit indexOptimized(indexId);
    }
    
    return success;
}

bool VectorIndexManager::compactIndex(const QString &indexId)
{
    qCDebug(vectorIndexManager) << "Compacting index:" << indexId;
    
    if (!validateIndexId(indexId)) {
        qCWarning(vectorIndexManager) << "Invalid index ID:" << indexId;
        return false;
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for compaction:" << indexId;
        return false;
    }
    
    // For now, compaction is the same as optimization
    // In a real implementation, this might involve more specific operations
    return index->optimize();
}

bool VectorIndexManager::validateIndexConsistency(const QString &indexId)
{
    qCDebug(vectorIndexManager) << "Validating index consistency:" << indexId;
    
    if (!validateIndexId(indexId)) {
        return false;
    }
    
    // Get the index
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for consistency validation:" << indexId;
        return false;
    }
    
    // Basic consistency checks
    bool consistent = true;
    
    // Check if index is properly initialized
    if (index->getDimension() <= 0) {
        qCWarning(vectorIndexManager) << "Index has invalid dimension:" << index->getDimension();
        consistent = false;
    }
    
    // Check if vector count is reasonable
    int vectorCount = index->getVectorCount();
    if (vectorCount < 0) {
        qCWarning(vectorIndexManager) << "Index has invalid vector count:" << vectorCount;
        consistent = false;
    }
    
    return consistent;
}

void VectorIndexManager::setCacheSize(qint64 sizeInBytes)
{
    qCDebug(vectorIndexManager) << "Setting cache size to:" << sizeInBytes << "bytes";
    m_maxCacheSize = sizeInBytes;
}

void VectorIndexManager::clearCache()
{
    qCDebug(vectorIndexManager) << "Clearing index cache";
    
    if (m_indexCache) {
        m_indexCache->clear();
    }
}

QVariantMap VectorIndexManager::getCacheStatistics() const
{
    QVariantMap stats;
    stats["maxSize"] = m_maxCacheSize;
    if (m_indexCache) {
        QVariantMap cacheStats = m_indexCache->getStatistics();
        stats["currentSize"] = cacheStats["currentSize"];
        stats["hitRate"] = cacheStats["hitRate"];
        stats["indexCount"] = cacheStats["indexCount"];
        stats["totalRequests"] = cacheStats["totalRequests"];
        stats["cacheHits"] = cacheStats["cacheHits"];
        stats["cacheMisses"] = cacheStats["cacheMisses"];
    } else {
        stats["currentSize"] = 0;
        stats["hitRate"] = 0.0;
        stats["indexCount"] = 0;
        stats["totalRequests"] = 0;
        stats["cacheHits"] = 0;
        stats["cacheMisses"] = 0;
    }
    
    return stats;
}

void VectorIndexManager::onMemoryPressure()
{
    qCDebug(vectorIndexManager) << "Handling memory pressure";
    evictLeastRecentlyUsedIndex();
}

void VectorIndexManager::onIndexOptimizationCompleted(const QString &indexId)
{
    qCDebug(vectorIndexManager) << "Index optimization completed:" << indexId;
    emit indexOptimized(indexId);
}

VectorIndexManager::IndexType VectorIndexManager::selectOptimalIndexType(int vectorCount, int dimension, const QVariantMap &requirements) const
{
    Q_UNUSED(requirements)
    
    // Simple heuristics for index type selection
    if (vectorCount < 1000) {
        return IndexType::Flat;
    } else if (vectorCount < 100000) {
        return IndexType::HNSW;
    } else if (dimension > 512) {
        return IndexType::IVFPQ;
    } else {
        return IndexType::IVF;
    }
}

QVariantMap VectorIndexManager::getOptimalIndexParams(IndexType type, int vectorCount, int dimension) const
{
    QVariantMap params;
    
    switch (type) {
    case IndexType::Flat:
        // No additional parameters needed
        break;
    case IndexType::IVF:
        params["nlist"] = qMin(4096, vectorCount / 39);
        break;
    case IndexType::HNSW:
        params["M"] = 16;
        params["efConstruction"] = 200;
        break;
    case IndexType::PQ:
        params["m"] = dimension / 8;
        params["nbits"] = 8;
        break;
    case IndexType::IVFPQ:
        params["nlist"] = qMin(4096, vectorCount / 39);
        params["m"] = dimension / 8;
        params["nbits"] = 8;
        break;
    }
    
    return params;
}

QString VectorIndexManager::getIndexFilePath(const QString &indexId) const
{
    return QString("%1/%2").arg(m_indexDir, indexId);
}

bool VectorIndexManager::saveIndexToFile(const QString &indexId)
{
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for saving:" << indexId;
        return false;
    }
    
    QString filePath = getIndexFilePath(indexId);    
    return index->saveToFile(filePath);
}

bool VectorIndexManager::loadIndexFromFile(const QString &indexId)
{
    QString filePath = getIndexFilePath(indexId);
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qCWarning(vectorIndexManager) << "Index file does not exist:" << filePath;
        return false;
    }

    // Create index wrapper，only create，after load from file
    QSharedPointer<FaissIndexWrapper> index = createFaissIndex(IndexType::Flat, 0, DistanceMetric::InnerProduct);
    if (!index) {
        qCWarning(vectorIndexManager) << "Failed to create index wrapper for loading";
        return false;
    }
    
    // Load from file
    if (!index->loadFromFile(filePath)) {
        qCWarning(vectorIndexManager) << "Failed to load index from file:" << filePath;
        return false;
    }
    
    // Add to cache
    if (m_indexCache) {
        m_indexCache->addIndex(indexId, index);
    }
    
    return true;
}

bool VectorIndexManager::verifyIndexConsistency(const QString &indexId) const
{
    // This is a duplicate of validateIndexConsistency - redirect to it
    return const_cast<VectorIndexManager*>(this)->validateIndexConsistency(indexId);
}

void VectorIndexManager::evictLeastRecentlyUsedIndex()
{
    if (m_indexCache) {
        m_indexCache->evictLeastRecentlyUsed();
    }
}

qint64 VectorIndexManager::calculateIndexMemoryUsage(const QString &indexId) const
{
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        return 0;
    }
    
    return index->getMemoryUsage();
}

bool VectorIndexManager::validateIndexId(const QString &indexId) const
{
    return !indexId.isEmpty() && indexId.length() <= 64;
}

bool VectorIndexManager::validateVectorId(const QString &vectorId) const
{
    return !vectorId.isEmpty() && vectorId.length() <= 64;
}

bool VectorIndexManager::validateVector(const QVector<float> &vector) const
{
    return !vector.isEmpty() && vector.size() <= 4096; // Reasonable upper limit
}

bool VectorIndexManager::validateConfig(const QVariantMap &config) const
{
    // Validate required fields
    if (!config.contains("type") || !config.contains("dimension")) {
        qCWarning(vectorIndexManager) << "Missing required config fields: type, dimension";
        return false;
    }
    
    int type = config.value("type").toInt();
    if (type < static_cast<int>(IndexType::Flat) || type > static_cast<int>(IndexType::IVFPQ)) {
        qCWarning(vectorIndexManager) << "Invalid index type:" << type;
        return false;
    }
    
    int dimension = config.value("dimension").toInt();
    if (dimension <= 0 || dimension > 10000) {
        qCWarning(vectorIndexManager) << "Invalid dimension:" << dimension;
        return false;
    }
    
    int metric = config.value("metric", static_cast<int>(DistanceMetric::InnerProduct)).toInt();
    if (metric < static_cast<int>(DistanceMetric::L2) || metric > static_cast<int>(DistanceMetric::Cosine)) {
        qCWarning(vectorIndexManager) << "Invalid distance metric:" << metric;
        return false;
    }
    
    return true;
}

QString VectorIndexManager::generateIndexId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QVariantList VectorIndexManager::searchInIndex(const QString &indexId, const QVector<float> &queryVector, int topK, const QVariantMap &searchParams)
{
    QSharedPointer<FaissIndexWrapper> index = getIndex(indexId);
    if (!index) {
        return QVariantList();
    }

    QList<FaissIndexWrapper::SearchResult> searchResults = index->search(queryVector, topK, searchParams);
    QVariantList results;
    for (const auto &result : searchResults) {
        QVariantMap res;
        res["id"] = result.vectorId;
        res["similarity"] = result.similarity;
        results.append(res);
    }

    return results;
}

QStringList VectorIndexManager::getAllIndexIds() const
{
    QStringList indexIds;
    QDir dir(m_indexDir);
    QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &file : files) {
        if (file.endsWith(".json"))
            continue;
        indexIds.append(file);
    }
    return indexIds;
}

void VectorIndexManager::initIndexDir(const QString &workDir)
{
    m_indexDir = workDir + QDir::separator() + "indexes";

    QDir dir(m_indexDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCWarning(vectorIndexManager) << "Failed to create storage directory:" << dir.absolutePath();
            return;
        }
    }
}

QSharedPointer<FaissIndexWrapper> VectorIndexManager::createFaissIndex(IndexType type, int dimension, DistanceMetric metric)
{
    QSharedPointer<FaissIndexWrapper> wrapper(new FaissIndexWrapper(type, dimension, metric, const_cast<VectorIndexManager*>(this)));
    
    // Initialize the wrapper
    QVariantMap config;
    config["type"] = static_cast<int>(type);
    config["dimension"] = dimension;
    config["metric"] = static_cast<int>(metric);
    
    if (!wrapper->initialize(config)) {
        qCWarning(vectorIndexManager) << "Failed to initialize FAISS index wrapper";
        return nullptr;
    }
    
    return wrapper;
}

bool VectorIndexManager::initializeIndex(const QString &indexId, const QVariantMap &config)
{
    // Extract configuration parameters
    IndexType type = static_cast<IndexType>(config.value("type", static_cast<int>(IndexType::Flat)).toInt());
    int dimension = config.value("dimension", 0).toInt();
    DistanceMetric metric = static_cast<DistanceMetric>(config.value("metric", static_cast<int>(DistanceMetric::InnerProduct)).toInt());
    
    // Create FAISS index wrapper
    QSharedPointer<FaissIndexWrapper> index = createFaissIndex(type, dimension, metric);
    if (!index) {
        qCWarning(vectorIndexManager) << "Failed to create FAISS index for:" << indexId;
        return false;
    }
    
    {
        QMutexLocker locker(&m_indexMutex);
        // Add to cache
        if (m_indexCache) {
            if (!m_indexCache->addIndex(indexId, index)) {
                qCWarning(vectorIndexManager) << "Failed to add index to cache:" << indexId;
                return false;
            }
        }

    }

    return true;
}

void VectorIndexManager::cleanupIndex(const QString &indexId)
{
    QMutexLocker locker(&m_indexMutex);
    
    // Remove from cache
    if (m_indexCache) {
        m_indexCache->removeIndex(indexId);
    }
    
    // Remove from indexes map
    m_indexes.remove(indexId);
}

QSharedPointer<FaissIndexWrapper> VectorIndexManager::getIndexFromCache(const QString &indexId) const
{
    if (m_indexCache) {
        return m_indexCache->getIndex(indexId);
    }
    
    // Fallback to direct lookup if no cache
    return m_indexes.value(indexId);
}

QSharedPointer<FaissIndexWrapper> VectorIndexManager::getIndex(const QString &indexId)
{
    QSharedPointer<FaissIndexWrapper> index = getIndexFromCache(indexId);
    if (!index) {
        qCWarning(vectorIndexManager) << "Index not found for cache, load file.";

        if (!loadIndexFromFile(indexId)) {
            qCWarning(vectorIndexManager) << "Index not found for cache and file, indexid:" << indexId;
            return nullptr;
        }

        index = getIndexFromCache(indexId);
        if (!index) {
            qCWarning(vectorIndexManager) << "Index get failed, indexid:" << indexId;
            return nullptr;
        }
    }

    return index;
}
