// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include "aidaemon_global.h"

#include <QObject>
#include <QScopedPointer>
#include <QHash>
#include <QMutex>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class DocumentDbWrapper;
class DocumentInfo;

/**
 * @brief Central data management coordinator
 * 
 * Manages document metadata, chunk data storage, retrieval, and backup operations.
 * Does not handle document parsing or text chunking - these are handled by external
 * processing modules. DataManager focuses purely on data persistence and management.
 */
class DataManager : public QObject
{
    Q_OBJECT

public:
    enum DocumentType {
        Text = 1,
        Image = 2,
        Audio = 3,
        Video = 4,
        PDF = 5,
        Office = 6
    };

    enum ProcessingStatus {
        Unprocessed = 0,
        Parsed = 1,
        Chunked = 2,
        Vectorized = 3
    };

    explicit DataManager(const QString &workDir);
    ~DataManager();

    /**
     * @brief Add a document to the project
     * @param filePath Document file path
     * @param metadata Document metadata
     * @return Document ID on success, empty string on failure
     */
    QString addDocument(const QString &filePath, QVariantMap metadata = QVariantMap());

    /**
     * @brief Remove a document from the project
     * @param documentId Document identifier
     * @return true on success, false on failure
     */
    bool removeDocument(const QString &documentId);

    /**
     * @brief Update document metadata
     * @param documentId Document identifier
     * @param metadata New metadata
     * @return true on success, false on failure
     */
    bool updateDocument(const QString &documentId, const QVariantMap &metadata);

    /**
     * @brief Update document processing status
     * @param documentId Document identifier
     * @param indexId New index identifier
     * @return true on success, false on failure
     */
    bool updateDocumentIndexId(const QString &documentId, const QString &indexId);

    QString getDocumentIndexId(const QString &documentId);

    /**
     * @brief Store parsed document content and metadata
     * @param documentId Document identifier
     * @param parsedContent Parsed text content
     * @param parseMetadata Metadata from parsing process
     * @return true on success, false on failure
     */
    bool storeDocumentContent(const QString &documentId, const QString &parsedContent, const QVariantMap &parseMetadata);

    /**
     * @brief Store document chunks
     * @param documentId Document identifier
     * @param chunks List of text chunks with metadata
     * @return true on success, false on failure
     */
    bool storeDocumentChunks(const QString &documentId, const QVariantList &chunks);

    /**
     * @brief Get document processing status
     * @param documentId Document identifier
     * @return Processing status information
     */
    QVariantMap getProcessingStatus(const QString &documentId) const;

    /**
     * @brief Get document information
     * @param documentId Document identifier
     * @return Document information as QVariantMap
     */
    QVariantMap getDocumentInfo(const QString &documentId) const;

    QStringList getDocumentPath(const QStringList &documentIds);

    /**
     * @brief Get all documents in the project
     * @return List of documents as QVariantList
     */
    QVariantList getAllDocuments() const;

    QStringList getAllDocumentsPath() const;

    /**
     * @brief Get documents by type
     * @param type Document type
     * @return List of documents as QVariantList
     */
    QVariantList getDocumentsByType(DocumentType type) const;

    /**
     * @brief Get documents by processing status
     * @param status Processing status
     * @return List of documents as QVariantList
     */
    QVariantList getDocumentsByStatus(ProcessingStatus status) const;

    /**
     * @brief Get parsed document content
     * @param documentId Document identifier
     * @return Parsed content as QString, empty if not found
     */
    QString getDocumentContent(const QString &documentId) const;

    /**
     * @brief Get chunks for a document
     * @param documentId Document identifier
     * @return List of chunks as QVariantList
     */
    QVariantList getDocumentChunks(const QString &documentId) const;

    /**
     * @brief Update chunk content
     * @param chunkId Chunk identifier
     * @param content New content
     * @return true on success, false on failure
     */
    bool updateChunkContent(const QString &chunkId, const QString &content);

    /**
     * @brief Search documents by query
     * @param query Search query
     * @param filters Search filters
     * @return List of matching documents
     */
    QVariantList searchDocuments(const QString &query, const QVariantMap &filters = QVariantMap()) const;

    /**
     * @brief Get document statistics
     * @return Statistics as QVariantMap
     */
    QVariantMap getDocumentStatistics() const;

    /**
     * @brief Verify data integrity
     * @return true if data is consistent, false otherwise
     */
    bool verifyDataIntegrity() const;

    bool updateProcessingStatus2Vector(const QStringList &documentIds);

    bool isReadyForProcessing(const QString &documentId) const;

    QVariantMap getChunkById(const QString &chunkId) const;

    QString getDocumentIdByPath(const QString &documentPath) const;

public Q_SLOTS:

Q_SIGNALS:
    /**
     * @brief Emitted when a document is added
     * @param documentId Document identifier
     */
    void documentAdded(const QString &documentId);

    /**
     * @brief Emitted when a document is removed
     * @param documentId Document identifier
     */
    void documentRemoved(const QString &documentId);

    /**
     * @brief Emitted when document data is stored
     * @param documentId Document identifier
     * @param success Whether storage succeeded
     */
    void documentDataStored(const QString &documentId, bool success);

    /**
     * @brief Emitted when document chunks are stored
     * @param documentId Document identifier
     * @param chunkCount Number of chunks stored
     */
    void documentChunksStored(const QString &documentId, int chunkCount);

    /**
     * @brief Emitted when an error occurs
     * @param operation Operation that failed
     * @param error Error message
     */
    void errorOccurred(const QString &operation, const QString &error);

private:
    QString m_workDir;
    
    // Core components
    QScopedPointer<DocumentDbWrapper> m_dbWrapper;
    
    // Helper methods
    bool validateDocumentPath(const QString &filePath) const;
    QString generateDocumentId() const;
    DocumentType detectDocumentType(const QString &filePath) const;
    QString calculateFileHash(const QString &filePath) const;
    bool updateProcessingStatus(const QString &documentId, ProcessingStatus status);
    bool initializeComponents();
    QString getDocumentPathById(const QString &id);
    int getDocumentNameCount(const QString &documentName) const;
    QString genBackupFileName(const QString &fileName) const;
    
    // File management
    QString getDocumentStorageDir() const;
    QString backupDocument(const QString &sourceFilePath, const QString &backupName);
    bool removeDocumentFromStorage(const QString &documentId);
    
    // Validation
    bool validateDocumentId(const QString &documentId) const;
    bool validateChunkId(const QString &chunkId) const;
    bool validateStorageConfig(const QVariantMap &config) const;
    bool validateChunkData(const QVariantList &chunks) const;

    QString calculateSHA256(const QString &filePath) const;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // DATAMANAGER_H
