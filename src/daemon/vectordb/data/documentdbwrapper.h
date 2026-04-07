// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOCUMENTDBWRAPPER_H
#define DOCUMENTDBWRAPPER_H

#include "aidaemon_global.h"

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtCore/QVariantList>
#include <QtCore/QStringList>
#include <QtCore/QDateTime>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Database wrapper for document storage and management
 * 
 * Provides database operations for storing document metadata,
 * processing status, and chunk information.
 */
class DocumentDbWrapper : public QObject
{
    Q_OBJECT

public:
    enum class ProcessingStatus {
        Unprocessed = 0,
        Parsed = 1,
        Chunked = 2,
        Vectorized = 3
    };
    Q_ENUM(ProcessingStatus)

    explicit DocumentDbWrapper(const QString &projectDir, QObject *parent = nullptr);
    ~DocumentDbWrapper();

    /**
     * @brief Initialize database connection and tables
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Insert a new document
     * @param documentId Document identifier
     * @param fileName Original file name
     * @param metadata Document metadata
     * @return true on success, false on failure
     */
    bool insertDocument(const QString &documentId, const QString &fileName, const QVariantMap &metadata);

    /**
     * @brief Update document metadata
     * @param documentId Document identifier
     * @param metadata New metadata
     * @return true on success, false on failure
     */
    bool updateDocument(const QString &documentId, const QVariantMap &metadata);

    /**
     * @brief Remove a document
     * @param documentId Document identifier
     * @return true on success, false on failure
     */
    bool removeDocument(const QString &documentId);

    /**
     * @brief Get document information
     * @param documentId Document identifier
     * @return Document information as QVariantMap
     */
    QVariantMap getDocument(const QString &documentId) const;

    /**
     * @brief Get all documents
     * @return List of documents as QVariantList
     */
    QVariantList getAllDocuments() const;

    /**
     * @brief Get documents by processing status
     * @param status Processing status
     * @return List of documents as QVariantList
     */
    QVariantList getDocumentsByStatus(ProcessingStatus status) const;

    /**
     * @brief Update document processing status
     * @param documentId Document identifier
     * @param status New processing status
     * @return true on success, false on failure
     */
    bool updateProcessingStatus(const QString &documentId, ProcessingStatus status);

    /**
     * @brief Insert document chunks
     * @param documentId Document identifier
     * @param chunks List of chunks
     * @return true on success, false on failure
     */
    bool insertChunks(const QString &documentId, const QVariantList &chunks);

    /**
     * @brief Get document chunks
     * @param documentId Document identifier
     * @return List of chunks as QVariantList
     */
    QVariantList getChunks(const QString &documentId) const;

    /**
     * @brief Get chunk by ID
     * @param chunkId Chunk identifier
     * @return Chunk content as QString
     */
    QVariantMap getChunk(const QString &chunkId) const;

    /**
     * @brief Update chunk content
     * @param chunkId Chunk identifier
     * @param content New content
     * @return true on success, false on failure
     */
    bool updateChunk(const QString &chunkId, const QString &content);

    /**
     * @brief Search documents by query
     * @param query Search query
     * @param filters Search filters
     * @return List of matching documents
     */
    QVariantList searchDocuments(const QString &query, const QVariantMap &filters = QVariantMap()) const;

    /**
     * @brief Update document index identifier
     * @param documentId Document identifier
     * @param indexId New index identifier
     * @return true on success, false on failure
     */
    bool updateDocumentIndexId(const QString &documentId, const QString &indexId);

    QString getDocumentIndexId(const QString &documentId);

    /**
     * @brief Get document statistics
     * @return Statistics as QVariantMap
     */
    QVariantMap getStatistics() const;

    /**
     * @brief Check if document exists
     * @param documentId Document identifier
     * @return true if exists, false otherwise
     */
    bool documentExists(const QString &documentId) const;

    int documentNameCount(const QString &documentPath) const;

    /**
     * @brief Get database file path
     * @return Database file path
     */
    QString getDatabasePath() const;

    QString getDocumentId(const QString &documentPath) const;

private:
    QString m_projectDir;
    QString m_connectionName;
    QSqlDatabase m_database;

    // Helper methods
    bool createTables();
    bool executeQuery(const QString &sql, const QVariantList &params = QVariantList()) const;
    QSqlQuery prepareQuery(const QString &sql) const;
    QString getConnectionName() const;
    QVariantMap queryToVariantMap(const QSqlQuery &query) const;
    QVariantList queryToVariantList(const QSqlQuery &query) const;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // DOCUMENTDBWRAPPER_H