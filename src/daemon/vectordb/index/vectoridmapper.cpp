// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vectoridmapper.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QMutexLocker>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(vectorIdMapper, "aidaemon.vectordb.vectoridmapper")

VectorIdMapper::VectorIdMapper(QObject *parent)
    : QObject(parent)
{
    qCDebug(vectorIdMapper) << "Creating vector ID mapper";
}

VectorIdMapper::~VectorIdMapper()
{
    qCDebug(vectorIdMapper) << "Destroying vector ID mapper with" << count() << "mappings";
}

int VectorIdMapper::addVectorId(const QString &vectorId)
{
    QMutexLocker locker(&m_mutex);
    
    if (vectorId.isEmpty()) {
        qCWarning(vectorIdMapper) << "Cannot add empty vector ID";
        return -1;
    }
    
    // Check if vector ID already exists
    if (m_vectorIdToIndex.contains(vectorId)) {
        qCDebug(vectorIdMapper) << "Vector ID already exists:" << vectorId;
        return m_vectorIdToIndex[vectorId];
    }
    
    // Assign new index
    int index = m_nextIndex++;
    m_vectorIdToIndex[vectorId] = index;
    m_indexToVectorId[index] = vectorId;
    
    qCDebug(vectorIdMapper) << "Added vector ID mapping:" << vectorId << "->" << index;
    return index;
}

bool VectorIdMapper::removeVectorId(const QString &vectorId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_vectorIdToIndex.contains(vectorId)) {
        qCDebug(vectorIdMapper) << "Vector ID not found for removal:" << vectorId;
        return false;
    }
    
    int index = m_vectorIdToIndex[vectorId];
    m_vectorIdToIndex.remove(vectorId);
    m_indexToVectorId.remove(index);
    
    qCDebug(vectorIdMapper) << "Removed vector ID mapping:" << vectorId << "(" << index << ")";
    return true;
}

int VectorIdMapper::getIndex(const QString &vectorId) const
{
    QMutexLocker locker(&m_mutex);
    return m_vectorIdToIndex.value(vectorId, -1);
}

QString VectorIdMapper::getVectorId(int index) const
{
    QMutexLocker locker(&m_mutex);
    return m_indexToVectorId.value(index, QString());
}

bool VectorIdMapper::hasVectorId(const QString &vectorId) const
{
    QMutexLocker locker(&m_mutex);
    return m_vectorIdToIndex.contains(vectorId);
}

bool VectorIdMapper::hasIndex(int index) const
{
    QMutexLocker locker(&m_mutex);
    return m_indexToVectorId.contains(index);
}

int VectorIdMapper::count() const
{
    return m_vectorIdToIndex.size();
}

void VectorIdMapper::clear()
{
    QMutexLocker locker(&m_mutex);
    
    qCDebug(vectorIdMapper) << "Clearing all vector ID mappings (" << count() << " mappings)";
    
    m_vectorIdToIndex.clear();
    m_indexToVectorId.clear();
    m_nextIndex = 0;
}

QStringList VectorIdMapper::getAllVectorIds() const
{
    QMutexLocker locker(&m_mutex);
    return m_vectorIdToIndex.keys();
}

QList<int> VectorIdMapper::getAllIndices() const
{
    QMutexLocker locker(&m_mutex);
    return m_indexToVectorId.keys();
}

qint64 VectorIdMapper::getMemoryUsage() const
{
    QMutexLocker locker(&m_mutex);
    
    qint64 usage = 0;
    
    // Hash table overhead + string storage for keys
    for (auto it = m_vectorIdToIndex.constBegin(); it != m_vectorIdToIndex.constEnd(); ++it) {
        usage += sizeof(int); // value
        usage += it.key().size() * sizeof(QChar); // key string data
        usage += sizeof(void*) * 2; // hash table node overhead
    }
    
    // Hash table overhead + string storage for values
    for (auto it = m_indexToVectorId.constBegin(); it != m_indexToVectorId.constEnd(); ++it) {
        usage += sizeof(int); // key
        usage += it.value().size() * sizeof(QChar); // value string data
        usage += sizeof(void*) * 2; // hash table node overhead
    }
    
    return usage;
}

bool VectorIdMapper::saveToFile(const QString &filePath) const
{
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(vectorIdMapper) << "Failed to open file for writing:" << filePath << "Error:" << file.errorString();
        return false;
    }
    
    QJsonObject rootObject;
    rootObject["nextIndex"] = m_nextIndex;
    
    QJsonArray mappingsArray;
    for (auto it = m_vectorIdToIndex.constBegin(); it != m_vectorIdToIndex.constEnd(); ++it) {
        QJsonObject mappingObject;
        mappingObject["vectorId"] = it.key();
        mappingObject["index"] = it.value();
        mappingsArray.append(mappingObject);
    }
    rootObject["mappings"] = mappingsArray;
    
    QJsonDocument doc(rootObject);
    QByteArray jsonData = doc.toJson(QJsonDocument::Indented);
    
    qint64 bytesWritten = file.write(jsonData);
    file.close();
    
    if (bytesWritten == -1) {
        qCWarning(vectorIdMapper) << "Failed to write data to file:" << filePath;
        return false;
    }
    
    qCDebug(vectorIdMapper) << "Successfully saved" << count() << "vector ID mappings to file:" << filePath;
    return true;
}

bool VectorIdMapper::loadFromFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(vectorIdMapper) << "Failed to open file for reading:" << filePath << "Error:" << file.errorString();
        return false;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) {
        qCWarning(vectorIdMapper) << "Failed to parse JSON from file:" << filePath;
        return false;
    }
    
    QJsonObject rootObject = doc.object();
    
    // Clear existing mappings
    m_vectorIdToIndex.clear();
    m_indexToVectorId.clear();
    
    m_nextIndex = rootObject.value("nextIndex").toInt(0);
    
    QJsonArray mappingsArray = rootObject.value("mappings").toArray();
    int loadedCount = 0;
    
    for (const QJsonValue &value : mappingsArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject mappingObject = value.toObject();
        QString vectorId = mappingObject.value("vectorId").toString();
        int index = mappingObject.value("index").toInt(-1);
        
        if (vectorId.isEmpty() || index < 0) {
            qCWarning(vectorIdMapper) << "Invalid mapping entry in file:" << filePath;
            continue;
        }
        
        m_vectorIdToIndex[vectorId] = index;
        m_indexToVectorId[index] = vectorId;
        loadedCount++;
    }
    
    // Ensure nextIndex is at least as large as the maximum loaded index + 1
    for (int index : m_indexToVectorId.keys()) {
        if (index >= m_nextIndex) {
            m_nextIndex = index + 1;
        }
    }
    
    qCDebug(vectorIdMapper) << "Successfully loaded" << loadedCount << "vector ID mappings from file:" << filePath;
    return true;
}

AIDAEMON_VECTORDB_END_NAMESPACE
