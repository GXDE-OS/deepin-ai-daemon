// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FAISSINDEXWRAPPER_H
#define FAISSINDEXWRAPPER_H

#include "aidaemon_global.h"
#include "vectoridmapper.h"

#include <QtCore/QObject>
#include <QtCore/QVector>
#include <QtCore/QVariantMap>
#include <QtCore/QVariantList>
#include <QtCore/QStringList>
#include <QtCore/QMutex>
#include <QtCore/QBitArray>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Wrapper for FAISS index operations
 * 
 * Provides a Qt-friendly interface to FAISS vector indexing functionality
 * with support for different index types and distance metrics.
 */
class FaissIndexWrapper : public QObject
{
    Q_OBJECT

public:
    enum class IndexType {
        Flat = 1,           // Brute force search
        IVF = 2,            // Inverted file index
        HNSW = 3,           // Hierarchical Navigable Small World
        PQ = 4,             // Product Quantization
        IVFPQ = 5           // IVF + Product Quantization
    };
    Q_ENUM(IndexType)

    enum class DistanceMetric {
        L2 = 1,             // Euclidean distance
        InnerProduct = 2,   // Inner product
        Cosine = 3          // Cosine distance
    };
    Q_ENUM(DistanceMetric)

    struct SearchResult {
        QString vectorId;
        float similarity;
        QVariantMap metadata;
    };

    explicit FaissIndexWrapper(IndexType type, int dimension, DistanceMetric metric, QObject *parent = nullptr);
    ~FaissIndexWrapper();

    /**
     * @brief Initialize the index with configuration
     * @param config Index configuration parameters
     * @return true on success, false on failure
     */
    bool initialize(const QVariantMap &config);

    /**
     * @brief Add vectors to the index
     * @param vectors List of vectors with metadata
     * @return true on success, false on failure
     */
    bool addVectors(const QVariantList &vectors);

    /**
     * @brief Remove vectors by marking them as deleted
     * @param vectorIds List of vector IDs to remove
     * @return true on success, false on failure
     */
    bool removeVectors(const QStringList &vectorIds);

    /**
     * @brief Search for similar vectors
     * @param queryVector Query vector
     * @param topK Number of results to return
     * @param params Search parameters
     * @return List of search results
     */
    QList<SearchResult> search(const QVector<float> &queryVector, int topK, const QVariantMap &params = QVariantMap());

    /**
     * @brief Batch search for multiple query vectors
     * @param queryVectors List of query vectors
     * @param topK Number of results per query
     * @return List of batch search results
     */
    QList<QList<SearchResult>> batchSearch(const QList<QVector<float>> &queryVectors, int topK);

    /**
     * @brief Range search within a radius
     * @param queryVector Query vector
     * @param radius Search radius
     * @return List of results within radius
     */
    QList<SearchResult> rangeSearch(const QVector<float> &queryVector, float radius);

    /**
     * @brief Train the index (for IVF-based indexes)
     * @param trainingVectors Training vectors
     * @return true on success, false on failure
     */
    bool train(const QList<QVector<float>> &trainingVectors);

    /**
     * @brief Save index to file
     * @param filePath File path to save to
     * @return true on success, false on failure
     */
    bool saveToFile(const QString &filePath);

    /**
     * @brief Load index from file
     * @param filePath File path to load from
     * @return true on success, false on failure
     */
    bool loadFromFile(const QString &filePath);

    /**
     * @brief Get number of vectors in index
     * @return Vector count
     */
    int getVectorCount() const;

    /**
     * @brief Get index dimension
     * @return Vector dimension
     */
    int getDimension() const { return m_dimension; }

    /**
     * @brief Get index type
     * @return Index type
     */
    IndexType getIndexType() const { return m_indexType; }

    /**
     * @brief Get distance metric
     * @return Distance metric
     */
    DistanceMetric getDistanceMetric() const { return m_metric; }

    /**
     * @brief Check if index is trained
     * @return true if trained, false otherwise
     */
    bool isTrained() const;

    /**
     * @brief Get memory usage in bytes
     * @return Memory usage
     */
    qint64 getMemoryUsage() const;

    /**
     * @brief Optimize index for better performance
     * @return true on success, false on failure
     */
    bool optimize();

    /**
     * @brief Reset index (clear all vectors)
     */
    void reset();

private:
    IndexType m_indexType;
    int m_dimension;
    DistanceMetric m_metric;
    QVariantMap m_config;
    
    // Vector ID mapping
    VectorIdMapper *m_idMapper;
    QBitArray m_deletionMask;
    
    // Thread safety
    mutable QMutex m_mutex;
    
    // FAISS index pointer (using void* to avoid FAISS dependency in header)
    void *m_faissIndex = nullptr;
    
    // Helper methods
    bool createFaissIndex();
    void cleanupFaissIndex();
    QString createIndexString() const;
    bool validateVector(const QVector<float> &vector) const;
    QVector<float> normalizeVector(const QVector<float> &vector) const;
    float calculateDistance(const QVector<float> &v1, const QVector<float> &v2) const;
    
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // FAISSINDEXWRAPPER_H
