// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "faissindexwrapper.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutexLocker>
#include <QtCore/QUuid>
#include <QtCore/QRandomGenerator>
#include <QtCore/QtMath>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <algorithm>

#include <faiss/IndexIVFPQ.h>
#include <faiss/index_io.h>
#include <faiss/index_factory.h>
#include <faiss/utils/random.h>
#include <faiss/IndexShards.h>
#include <faiss/IndexFlatCodes.h>
#include <faiss/impl/IDSelector.h>
#include <faiss/IndexIDMap.h>
#include <faiss/IndexFlat.h>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(faissIndexWrapper, "aidaemon.vectordb.faissindexwrapper")

FaissIndexWrapper::FaissIndexWrapper(IndexType type, int dimension, DistanceMetric metric, QObject *parent)
    : QObject(parent)
    , m_indexType(type)
    , m_dimension(dimension)
    , m_metric(metric)
    , m_idMapper(new VectorIdMapper(this))
{
    qCDebug(faissIndexWrapper) << "Creating FAISS index wrapper:"
                               << "type=" << static_cast<int>(type)
                               << "dimension=" << dimension
                               << "metric=" << static_cast<int>(metric);
}

FaissIndexWrapper::~FaissIndexWrapper()
{
    cleanupFaissIndex();
}

bool FaissIndexWrapper::initialize(const QVariantMap &config)
{
    QMutexLocker locker(&m_mutex);

    qCDebug(faissIndexWrapper) << "Initializing FAISS index with config:" << config;
    
    m_config = config;
    
    // Clean up existing index if any
    cleanupFaissIndex();
    
    // Reset state
    m_idMapper->clear();
    m_deletionMask.clear();
    
    // Create FAISS index
    if (!createFaissIndex()) {
        qCCritical(faissIndexWrapper) << "Failed to create FAISS index";
        return false;
    }
    
    qCDebug(faissIndexWrapper) << "FAISS index initialized successfully";
    return true;
}

bool FaissIndexWrapper::addVectors(const QVariantList &vectors)
{
    QMutexLocker locker(&m_mutex);
    
    if (vectors.isEmpty()) {
        qCWarning(faissIndexWrapper) << "No vectors to add";
        return false;
    }
    
    qCDebug(faissIndexWrapper) << "Adding" << vectors.size() << "vectors to index";
    
    // Parse and validate vectors
    QVector<float> allVectors;
    QVector<faiss::idx_t> vectorIds;
    QList<QString> stringIds;
    
    for (const QVariant &vectorData : vectors) {
        QVariantMap vectorMap = vectorData.toMap();
        QString vectorId = vectorMap.value("vectorId").toString();
        QVariantList vectorList = vectorMap.value("vector").toList();
        
        if (vectorId.isEmpty()) {
            return false;
        }
        
        // Convert QVariantList to QVector<float>
        QVector<float> vector;
        vector.reserve(vectorList.size());
        for (const QVariant &value : vectorList) {
            vector.append(value.toFloat());
        }
        
        if (!validateVector(vector)) {
            qCWarning(faissIndexWrapper) << "Invalid vector dimension for vector" << vectorId;
            continue;
        }
        
        // Normalize vector if using cosine distance
        if (m_metric == DistanceMetric::InnerProduct) {
            vector = normalizeVector(vector);
        }
        
        // Use VectorIdMapper for ID mapping
        int numericId = m_idMapper->addVectorId(vectorId);
        if (numericId < 0) {
            qCWarning(faissIndexWrapper) << "Failed to map vector ID:" << vectorId;
            continue;
        }
        
        // Collect data for batch addition
        allVectors += vector;
        vectorIds.append(static_cast<faiss::idx_t>(numericId));
        stringIds.append(vectorId);
    }
    
    if (vectorIds.isEmpty()) {
        qCWarning(faissIndexWrapper) << "No valid vectors to add";
        return false;
    }
    
    // FAISS implementation
    if (!m_faissIndex) {
        qCWarning(faissIndexWrapper) << "FAISS index not initialized";
        return false;
    }
    
    try {
        faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
        idMapIndex->add_with_ids(vectorIds.size(), allVectors.data(), vectorIds.data());
        
        // Extend deletion mask if needed
        int maxId = *std::max_element(vectorIds.begin(), vectorIds.end());
        if (m_deletionMask.size() <= maxId) {
            int oldSize = m_deletionMask.size();
            m_deletionMask.resize(maxId + 1);
            // Initialize new bits to false (not deleted)
            for (int i = oldSize; i <= maxId; ++i) {
                m_deletionMask.setBit(i, false);
            }
        }
        
        qCDebug(faissIndexWrapper) << "Successfully added" << vectorIds.size() << "vectors to FAISS index";
        return true;
    } catch (const faiss::FaissException &e) {
        qCCritical(faissIndexWrapper) << "Failed to add vectors to FAISS index:" << e.what();
        return false;
    }
}

bool FaissIndexWrapper::removeVectors(const QStringList &vectorIds)
{
    QMutexLocker locker(&m_mutex);
    
    if (vectorIds.isEmpty()) {
        qCWarning(faissIndexWrapper) << "No vector IDs to remove";
        return false;
    }
    
    qCDebug(faissIndexWrapper) << "Removing" << vectorIds.size() << "vectors from index";
    
    int removedCount = 0;
    
    for (const QString &vectorId : vectorIds) {
        if (!m_idMapper->hasVectorId(vectorId)) {
            qCDebug(faissIndexWrapper) << "Vector ID not found:" << vectorId;
            continue;
        }
        
        int numericId = m_idMapper->getIndex(vectorId);
        if (numericId < 0) {
            qCWarning(faissIndexWrapper) << "Invalid numeric ID for vector:" << vectorId;
            continue;
        }
        
        // Remove from ID mapper
        if (m_idMapper->removeVectorId(vectorId)) {
            removedCount++;
        }
    }
    
    qCDebug(faissIndexWrapper) << "Successfully marked" << removedCount << "vectors as deleted";
    return removedCount > 0;
}

QList<FaissIndexWrapper::SearchResult> FaissIndexWrapper::search(const QVector<float> &queryVector, int topK, const QVariantMap &params)
{
    QMutexLocker locker(&m_mutex);
    
    Q_UNUSED(params)
    
    if (!validateVector(queryVector)) {
        qCWarning(faissIndexWrapper) << "Invalid query vector dimension";
        return {};
    }
    
    if (topK <= 0) {
        qCWarning(faissIndexWrapper) << "Invalid topK value:" << topK;
        return {};
    }
    
    qCDebug(faissIndexWrapper) << "Searching for" << topK << "similar vectors";
    
    // FAISS implementation
    if (!m_faissIndex) {
        qCWarning(faissIndexWrapper) << "FAISS index not initialized";
        return {};
    }
    
    try {
        faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
        
        QVector<float> normalizedQuery = queryVector;
        if (m_metric == DistanceMetric::InnerProduct) {
            normalizedQuery = normalizeVector(queryVector);
        }
        
        QVector<float> similarities(topK);
        QVector<faiss::idx_t> indices(topK);
        
        // Create deletion selector if we have deleted vectors
        faiss::SearchParameters *searchParams = nullptr;
        faiss::IDSelectorBitmap *selector = nullptr;
        
        if (m_deletionMask.count(true) > 0) {
            // Convert QBitArray to uint8_t array (inverted - 0 means keep, 1 means delete)
            int bitmapSize = (m_deletionMask.size() + 7) / 8;
            QVector<uint8_t> bitmap(bitmapSize, 0);
            
            for (int i = 0; i < m_deletionMask.size(); ++i) {
                if (!m_deletionMask.testBit(i)) { // Keep this vector (not deleted)
                    bitmap[i / 8] |= (1 << (i % 8));
                }
            }
            
            selector = new faiss::IDSelectorBitmap(m_deletionMask.size(), bitmap.data());
            searchParams = new faiss::SearchParameters();
            searchParams->sel = selector;
        }
        
        idMapIndex->search(1, normalizedQuery.data(), topK, similarities.data(), indices.data(), searchParams);
        
        // Clean up
        delete searchParams;
        delete selector;
        
        // Convert results using VectorIdMapper
        QList<SearchResult> results;
        for (int i = 0; i < topK; ++i) {
            if (indices[i] == -1) {
                break; // Invalid result
            }
            
            faiss::idx_t numericId = indices[i];
            QString vectorId = m_idMapper->getVectorId(static_cast<int>(numericId));
            if (vectorId.isEmpty()) {
                continue; // ID not found in mapping
            }
            
            SearchResult result;
            result.vectorId = vectorId;
            result.similarity = similarities[i];
            // Note: metadata is not stored in FAISS, would need separate storage
            results.append(result);
        }
        
        qCDebug(faissIndexWrapper) << "FAISS search returned" << results.size() << "results";
        return results;
        
    } catch (const faiss::FaissException &e) {
        qCCritical(faissIndexWrapper) << "FAISS search failed:" << e.what();
        return {};
    }
}

QList<QList<FaissIndexWrapper::SearchResult>> FaissIndexWrapper::batchSearch(const QList<QVector<float>> &queryVectors, int topK)
{
    QMutexLocker locker(&m_mutex);
    
    QList<QList<SearchResult>> results;
    results.reserve(queryVectors.size());
    
    for (const QVector<float> &queryVector : queryVectors) {
        results.append(search(queryVector, topK));
    }
    
    return results;
}

QList<FaissIndexWrapper::SearchResult> FaissIndexWrapper::rangeSearch(const QVector<float> &queryVector, float radius)
{
    QMutexLocker locker(&m_mutex);
    
    if (!validateVector(queryVector)) {
        qCWarning(faissIndexWrapper) << "Invalid query vector dimension";
        return {};
    }
    
    if (radius <= 0.0f) {
        qCWarning(faissIndexWrapper) << "Invalid radius value:" << radius;
        return {};
    }
    
    qCDebug(faissIndexWrapper) << "Range search with radius:" << radius;
    
    // TODO: Implement FAISS range search
    Q_UNUSED(queryVector)
    Q_UNUSED(radius)
    qCWarning(faissIndexWrapper) << "Range search not yet implemented for FAISS";
    return {};
}

bool FaissIndexWrapper::train(const QList<QVector<float>> &trainingVectors)
{
    QMutexLocker locker(&m_mutex);
    
    qCDebug(faissIndexWrapper) << "Training index with" << trainingVectors.size() << "vectors";
    
    // FAISS implementation
    if (!m_faissIndex) {
        qCWarning(faissIndexWrapper) << "FAISS index not initialized";
        return false;
    }
    
    try {
        faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
        faiss::Index *baseIndex = idMapIndex->index;
        
        // Check if training is needed
        if (baseIndex->is_trained) {
            qCDebug(faissIndexWrapper) << "Index is already trained";
            return true;
        }
        
        // Prepare training data
        QVector<float> trainingData;
        for (const QVector<float> &vector : trainingVectors) {
            if (validateVector(vector)) {
                QVector<float> processedVector = vector;
                if (m_metric == DistanceMetric::Cosine) {
                    processedVector = normalizeVector(vector);
                }
                trainingData += processedVector;
            }
        }
        
        if (trainingData.isEmpty()) {
            qCWarning(faissIndexWrapper) << "No valid training vectors provided";
            return false;
        }
        
        int numVectors = trainingData.size() / m_dimension;
        qCDebug(faissIndexWrapper) << "Training FAISS index with" << numVectors << "vectors";
        
        baseIndex->train(numVectors, trainingData.data());
        
        qCDebug(faissIndexWrapper) << "FAISS index training completed";
        return true;
        
    } catch (const faiss::FaissException &e) {
        qCCritical(faissIndexWrapper) << "FAISS training failed:" << e.what();
        return false;
    }
}

bool FaissIndexWrapper::saveToFile(const QString &filePath)
{
    qCDebug(faissIndexWrapper) << "Saving index to file:" << filePath;
    
    
    if (!m_faissIndex) {
        qCWarning(faissIndexWrapper) << "FAISS index not initialized";
        return false;
    }
    
    // Ensure directory exists
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(dir.absolutePath())) {
            qCWarning(faissIndexWrapper) << "Failed to create directory:" << dir.absolutePath();
            return false;
        }
    }
    
    try {
        faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
        faiss::write_index(idMapIndex, filePath.toStdString().c_str());
        
        m_idMapper->saveToFile(filePath + "_idmap.json");
        qCDebug(faissIndexWrapper) << "Successfully saved FAISS index to:" << filePath;
        return true;
        
    } catch (const faiss::FaissException &e) {
        qCCritical(faissIndexWrapper) << "Failed to save FAISS index:" << e.what();
        return false;
    }
}

bool FaissIndexWrapper::loadFromFile(const QString &filePath)
{
    qCDebug(faissIndexWrapper) << "Loading index from file:" << filePath;
    
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qCWarning(faissIndexWrapper) << "Index file does not exist:" << filePath;
        return false;
    }
    
    try {
        // Clean up existing index
        cleanupFaissIndex();
        
        // Load the index
        faiss::Index *loadedIndex = faiss::read_index(filePath.toStdString().c_str());
        if (loadedIndex) {
            // Set config
            m_dimension = loadedIndex->d;
            faiss::MetricType metricType = loadedIndex->metric_type;
            if (faiss::MetricType::METRIC_L2 == metricType) {
                m_metric = DistanceMetric::L2;
            } else if (faiss::MetricType::METRIC_INNER_PRODUCT == metricType) {
                m_metric = DistanceMetric::InnerProduct;
            }
        }
        
        // Check if it's already an IndexIDMap
        faiss::IndexIDMap *idMapIndex = dynamic_cast<faiss::IndexIDMap*>(loadedIndex);
        if (idMapIndex) {
            m_faissIndex = idMapIndex;
        } else {
            // Wrap in IndexIDMap if it's not already
            m_faissIndex = new faiss::IndexIDMap(loadedIndex);
        }
        
        m_idMapper->loadFromFile(filePath + "_idmap.json");
        qCDebug(faissIndexWrapper) << "Successfully loaded FAISS index from:" << filePath;
        qCDebug(faissIndexWrapper) << "Index contains" << getVectorCount() << "vectors";
        
        return true;
        
    } catch (const faiss::FaissException &e) {
        qCCritical(faissIndexWrapper) << "Failed to load FAISS index:" << e.what();
        return false;
    }
}

int FaissIndexWrapper::getVectorCount() const
{
    QMutexLocker locker(&m_mutex);
    
    return m_idMapper ? m_idMapper->count() : 0;
}

bool FaissIndexWrapper::isTrained() const
{
    QMutexLocker locker(&m_mutex);
    
    
    if (!m_faissIndex) {
        return false;
    }
    
    faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
    return idMapIndex->is_trained;
}

qint64 FaissIndexWrapper::getMemoryUsage() const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_faissIndex) {
        return 0;
    }
    
    // Estimate FAISS memory usage
    faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
    qint64 usage = 0;
    
    // Base index memory
    usage += idMapIndex->ntotal * m_dimension * sizeof(float);
    
    // ID mapping memory
    usage += idMapIndex->id_map.size() * sizeof(faiss::idx_t);
    
    // VectorIdMapper memory
    if (m_idMapper) {
        usage += m_idMapper->getMemoryUsage();
    }
    
    return usage;
}

bool FaissIndexWrapper::optimize()
{
    QMutexLocker locker(&m_mutex);
    
    qCDebug(faissIndexWrapper) << "Optimizing index";
    
    
    if (!m_faissIndex) {
        qCWarning(faissIndexWrapper) << "FAISS index not initialized";
        return false;
    }
    
    // For now, optimization is a no-op for FAISS as well
    // In a real implementation, this could:
    // - Rebuild the index with better parameters
    // - Compress the index
    // - Remove deleted vectors permanently
    qCDebug(faissIndexWrapper) << "Index optimization completed";
    return true;
}

void FaissIndexWrapper::reset()
{
    QMutexLocker locker(&m_mutex);
    
    qCDebug(faissIndexWrapper) << "Resetting index";
    
    if (m_idMapper) {
        m_idMapper->clear();
    }
    
    cleanupFaissIndex();
    createFaissIndex();
}

bool FaissIndexWrapper::createFaissIndex()
{
    try {
        QString indexString = createIndexString();
        qCDebug(faissIndexWrapper) << "Creating FAISS index with string:" << indexString;
        
        // Create the base index using factory
        faiss::MetricType metricType = faiss::MetricType::METRIC_L2;
        if (DistanceMetric::InnerProduct == m_metric) {
            metricType = faiss::MetricType::METRIC_INNER_PRODUCT;
        }
        faiss::Index *baseIndex = faiss::index_factory(m_dimension, indexString.toStdString().c_str(), metricType);
        
        if (!baseIndex) {
            qCWarning(faissIndexWrapper) << "Failed to create FAISS index";
            return false;
        }
        
        // Wrap in IndexIDMap for ID support
        m_faissIndex = new faiss::IndexIDMap(baseIndex);
        
        qCDebug(faissIndexWrapper) << "Successfully created FAISS index";
        return true;
        
    } catch (const faiss::FaissException &e) {
        qCCritical(faissIndexWrapper) << "Failed to create FAISS index:" << e.what();
        return false;
    } catch (const std::exception &e) {
        qCCritical(faissIndexWrapper) << "Failed to create FAISS index:" << e.what();
        return false;
    }
}

void FaissIndexWrapper::cleanupFaissIndex()
{
    if (m_faissIndex) {
        faiss::IndexIDMap *idMapIndex = static_cast<faiss::IndexIDMap*>(m_faissIndex);
        delete idMapIndex; // This will also delete the wrapped base index
        m_faissIndex = nullptr;
    }
}

QString FaissIndexWrapper::createIndexString() const
{
    QString indexString;
    
    switch (m_indexType) {
    case IndexType::Flat:
        indexString = "Flat";
        break;
    case IndexType::IVF:
        indexString = QString("IVF%1,Flat").arg(m_config.value("nlist", 100).toInt());
        break;
    case IndexType::HNSW:
        indexString = QString("HNSW%1").arg(m_config.value("M", 16).toInt());
        break;
    case IndexType::PQ:
        indexString = QString("PQ%1").arg(m_config.value("m", 8).toInt());
        break;
    case IndexType::IVFPQ:
        indexString = QString("IVF%1,PQ%2")
                          .arg(m_config.value("nlist", 100).toInt())
                          .arg(m_config.value("m", 8).toInt());
        break;
    }
    
    // Add distance metric if not L2 (which is default)
    if (m_metric == DistanceMetric::InnerProduct) {
        // For inner product, we need to use a different index type
        // FAISS uses negative inner product as distance
        // We'll handle this in the vector processing
    } else if (m_metric == DistanceMetric::Cosine) {
        // For cosine distance, we normalize vectors and use L2 distance
        // The normalization is handled in vector processing
    }
    
    return indexString;
}

bool FaissIndexWrapper::validateVector(const QVector<float> &vector) const
{
    return vector.size() == m_dimension;
}

QVector<float> FaissIndexWrapper::normalizeVector(const QVector<float> &vector) const
{
    float norm = 0.0f;
    for (float value : vector) {
        norm += value * value;
    }
    norm = qSqrt(norm);
    
    if (norm == 0.0f) {
        return vector; // Avoid division by zero
    }
    
    QVector<float> normalized;
    normalized.reserve(vector.size());
    for (float value : vector) {
        normalized.append(value / norm);
    }
    
    return normalized;
}

float FaissIndexWrapper::calculateDistance(const QVector<float> &v1, const QVector<float> &v2) const
{
    if (v1.size() != v2.size()) {
        return std::numeric_limits<float>::max();
    }
    
    switch (m_metric) {
    case DistanceMetric::L2: {
        float distance = 0.0f;
        for (int i = 0; i < v1.size(); ++i) {
            float diff = v1[i] - v2[i];
            distance += diff * diff;
        }
        return qSqrt(distance);
    }
    case DistanceMetric::InnerProduct: {
        float product = 0.0f;
        for (int i = 0; i < v1.size(); ++i) {
            product += v1[i] * v2[i];
        }
        return -product; // Negative because we want higher similarity to have lower distance
    }
    case DistanceMetric::Cosine: {
        float dotProduct = 0.0f;
        for (int i = 0; i < v1.size(); ++i) {
            dotProduct += v1[i] * v2[i];
        }
        return 1.0f - dotProduct; // Cosine distance = 1 - cosine similarity
    }
    }
    
    return std::numeric_limits<float>::max();
}


AIDAEMON_VECTORDB_END_NAMESPACE
