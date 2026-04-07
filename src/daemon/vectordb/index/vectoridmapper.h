// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORIDMAPPER_H
#define VECTORIDMAPPER_H

#include "aidaemon_global.h"

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QString>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Maps between vector IDs and internal index positions
 * 
 * Provides bidirectional mapping between string vector IDs and
 * internal integer indices used by FAISS.
 */
class VectorIdMapper : public QObject
{
    Q_OBJECT

public:
    explicit VectorIdMapper(QObject *parent = nullptr);
    ~VectorIdMapper();

    /**
     * @brief Add a new vector ID mapping
     * @param vectorId Vector identifier
     * @return Internal index assigned to the vector
     */
    int addVectorId(const QString &vectorId);

    /**
     * @brief Remove a vector ID mapping
     * @param vectorId Vector identifier
     * @return true if removed, false if not found
     */
    bool removeVectorId(const QString &vectorId);

    /**
     * @brief Get internal index for a vector ID
     * @param vectorId Vector identifier
     * @return Internal index, or -1 if not found
     */
    int getIndex(const QString &vectorId) const;

    /**
     * @brief Get vector ID for an internal index
     * @param index Internal index
     * @return Vector ID, or empty string if not found
     */
    QString getVectorId(int index) const;

    /**
     * @brief Check if vector ID exists
     * @param vectorId Vector identifier
     * @return true if exists, false otherwise
     */
    bool hasVectorId(const QString &vectorId) const;

    /**
     * @brief Check if internal index exists
     * @param index Internal index
     * @return true if exists, false otherwise
     */
    bool hasIndex(int index) const;

    /**
     * @brief Get total number of mappings
     * @return Number of vector ID mappings
     */
    int count() const;

    /**
     * @brief Clear all mappings
     */
    void clear();

    /**
     * @brief Get all vector IDs
     * @return List of all vector IDs
     */
    QStringList getAllVectorIds() const;

    /**
     * @brief Get all internal indices
     * @return List of all internal indices
     */
    QList<int> getAllIndices() const;

    /**
     * @brief Get memory usage estimate
     * @return Estimated memory usage in bytes
     */
    qint64 getMemoryUsage() const;

    /**
     * @brief Save ID mappings to file
     * @param filePath Path to the file
     * @return true if saved successfully, false otherwise
     */
    bool saveToFile(const QString &filePath) const;

    /**
     * @brief Load ID mappings from file
     * @param filePath Path to the file
     * @return true if loaded successfully, false otherwise
     */
    bool loadFromFile(const QString &filePath);

private:
    mutable QMutex m_mutex;
    QHash<QString, int> m_vectorIdToIndex;
    QHash<int, QString> m_indexToVectorId;
    int m_nextIndex = 0;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORIDMAPPER_H