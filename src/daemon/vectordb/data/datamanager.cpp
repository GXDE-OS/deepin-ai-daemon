// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "datamanager.h"
#include "documentdbwrapper.h"
#include "documentinfo.h"

#include <QtCore/QLoggingCategory>
#include <QUuid>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QCryptographicHash>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QDir>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(dataManager, "vectordb.data.manager")

DataManager::DataManager(const QString &workDir)
    : QObject(nullptr)
    , m_workDir(workDir)
{
    qCDebug(dataManager) << "DataManager created for work directory:" << workDir;

    initializeComponents();
}

DataManager::~DataManager()
{
    qCDebug(dataManager) << "DataManager destroyed";
}

QString DataManager::addDocument(const QString &filePath, QVariantMap metadata)
{
    qCDebug(dataManager) << "Adding document:" << filePath << "to work directory:" << m_workDir;

    if (!validateDocumentPath(filePath)) {
        qCWarning(dataManager) << "Invalid document path:" << filePath;
        return QString();
    }

    // Generate document ID
    QString documentId = generateDocumentId();
    QString backupName = genBackupFileName(filePath);
    QString backupPath = backupDocument(filePath, backupName);
    // Copy document to storage
    if (backupPath.isEmpty()) {
        qCWarning(dataManager) << "Failed to copy document to storage:" << filePath;
        return QString();
    }

    // Initialize components if not already done
    if (!m_dbWrapper) {
        if (!initializeComponents()) {
            qCWarning(dataManager) << "Failed to initialize components";
            return QString();
        }
    }

    metadata.insert("sha256", calculateSHA256(backupPath));

    // Insert document into database
    if (!m_dbWrapper->insertDocument(documentId, backupName, metadata)) {
        qCWarning(dataManager) << "Failed to insert document into database";
        return QString();
    }

    emit documentAdded(documentId);

    qCDebug(dataManager) << "Document added successfully:" << documentId;
    return documentId;
}

bool DataManager::removeDocument(const QString &documentId)
{
    qCDebug(dataManager) << "Removing document:" << documentId << "from project:" << m_workDir;

    if (!validateDocumentId(documentId)) {
        qCWarning(dataManager) << "Invalid document ID:" << documentId;
        return false;
    }

    // remove document from storage
    removeDocumentFromStorage(documentId);

    // Remove document from database
    if (m_dbWrapper && !m_dbWrapper->removeDocument(documentId)) {
        qCWarning(dataManager) << "Failed to remove document from database";
        return false;
    }

    emit documentRemoved(documentId);

    qCDebug(dataManager) << "Document removed successfully:" << documentId;
    return true;
}

bool DataManager::updateDocument(const QString &documentId, const QVariantMap &metadata)
{
    qCDebug(dataManager) << "Updating document:" << documentId;

    if (!validateDocumentId(documentId)) {
        qCWarning(dataManager) << "Invalid document ID:" << documentId;
        return false;
    }

    // Update document in database
    if (m_dbWrapper && !m_dbWrapper->updateDocument(documentId, metadata)) {
        qCWarning(dataManager) << "Failed to update document in database";
        return false;
    }

    return true;
}

bool DataManager::updateDocumentIndexId(const QString &documentId, const QString &indexId)
{
    qCDebug(dataManager) << "Updating document index ID:" << documentId << "to:" << indexId;
    if (!m_dbWrapper) {
        qCWarning(dataManager) << "Database wrapper not initialized";
        return false;
    }

    if (!m_dbWrapper->updateDocumentIndexId(documentId, indexId)) {
        qCWarning(dataManager) << "Failed to update document index ID in database";
        return false;
    }

    return true;
}

QString DataManager::getDocumentIndexId(const QString &documentId)
{
    if (!m_dbWrapper) {
        qCWarning(dataManager) << "Database wrapper not initialized";
        return QString();
    }
    return m_dbWrapper->getDocumentIndexId(documentId);
}

QVariantMap DataManager::getProcessingStatus(const QString &documentId) const
{
    if (!validateDocumentId(documentId)) {
        return QVariantMap();
    }

    QVariantMap status;
    status["documentId"] = documentId;

    if (m_dbWrapper) {
        QVariantMap docInfo = m_dbWrapper->getDocument(documentId);
        if (!docInfo.isEmpty()) {
            status["status"] = docInfo.value("processing_status", static_cast<int>(ProcessingStatus::Unprocessed));
            status["filePath"] = docInfo.value("file_path");
            status["metadata"] = docInfo.value("metadata");
            status["createdAt"] = docInfo.value("created_at");
            status["updatedAt"] = docInfo.value("updated_at");
        } else {
            status["status"] = static_cast<int>(ProcessingStatus::Unprocessed);
        }
    } else {
        status["status"] = static_cast<int>(ProcessingStatus::Unprocessed);
    }
    status["progress"] = 0;

    return status;
}

QVariantMap DataManager::getDocumentInfo(const QString &documentId) const
{
    if (!validateDocumentId(documentId))
        return QVariantMap();

    // Get from database
    QVariantMap info;
    if (m_dbWrapper) {
        info = m_dbWrapper->getDocument(documentId);
    }

    return info;
}

QStringList DataManager::getDocumentPath(const QStringList &documentIds)
{
    QStringList results;
    for (const QString &id : documentIds) {
        results.append(getDocumentPathById(id));
    }

    return results;
}

QVariantList DataManager::getAllDocuments() const
{
    if (m_dbWrapper) {
        return m_dbWrapper->getAllDocuments();
    }
    return QVariantList();
}

QStringList DataManager::getAllDocumentsPath() const
{
    if (!m_dbWrapper) {
        return QStringList();
    }

    QVariantList docInfo = m_dbWrapper->getAllDocuments();

    QStringList filePaths;
    for (auto doc : docInfo) {
        filePaths.append(getDocumentStorageDir() + QDir::separator() + doc.toMap().value("file_path").toString());
    }

    return filePaths;
}

QVariantList DataManager::getDocumentsByType(DocumentType type) const
{
    Q_UNUSED(type)

    // For now, get all documents and filter by type
    // In a real implementation, this would be done at the database level
    QVariantList allDocs = getAllDocuments();
    QVariantList filteredDocs;

    for (const QVariant &docVariant : allDocs) {
        QVariantMap doc = docVariant.toMap();
        DocumentType docType = static_cast<DocumentType>(doc.value("type", static_cast<int>(DocumentType::Text)).toInt());
        if (docType == type) {
            filteredDocs.append(doc);
        }
    }

    return filteredDocs;
}

QVariantList DataManager::getDocumentsByStatus(ProcessingStatus status) const
{
    Q_UNUSED(status)

    if (m_dbWrapper) {
        DocumentDbWrapper::ProcessingStatus dbStatus = static_cast<DocumentDbWrapper::ProcessingStatus>(status);
        return m_dbWrapper->getDocumentsByStatus(dbStatus);
    }
    return QVariantList();
}

QString DataManager::getDocumentContent(const QString &documentId) const
{
    qCDebug(dataManager) << "Getting document content for:" << documentId;

    if (!validateDocumentId(documentId)) {
        qCWarning(dataManager) << "Invalid document ID:" << documentId;
        return QString();
    }

    if (!m_dbWrapper) {
        qCWarning(dataManager) << "Database wrapper not initialized";
        return QString();
    }

    // Get document info from database
    QVariantMap docInfo = m_dbWrapper->getDocument(documentId);
    if (docInfo.isEmpty()) {
        qCWarning(dataManager) << "Document not found:" << documentId;
        return QString();
    }

    // Return parsed content if available
    QString parsedContent = docInfo.value("parsed_content").toString();
    if (parsedContent.isEmpty()) {
        qCDebug(dataManager) << "No parsed content available for document:" << documentId;
    }

    return parsedContent;
}

QVariantList DataManager::getDocumentChunks(const QString &documentId) const
{
    if (!validateDocumentId(documentId)) {
        return QVariantList();
    }

    if (m_dbWrapper) {
        return m_dbWrapper->getChunks(documentId);
    }
    return QVariantList();
}

bool DataManager::updateChunkContent(const QString &chunkId, const QString &content)
{
    qCDebug(dataManager) << "Updating chunk content:" << chunkId;

    if (!validateChunkId(chunkId) || content.isEmpty()) {
        qCWarning(dataManager) << "Invalid parameters for updateChunkContent";
        return false;
    }

    if (m_dbWrapper) {
        return m_dbWrapper->updateChunk(chunkId, content);
    }
    return false;
}

QVariantList DataManager::searchDocuments(const QString &query, const QVariantMap &filters) const
{
    qCDebug(dataManager) << "Searching documents with query:" << query;

    if (query.isEmpty()) {
        return QVariantList();
    }

    if (m_dbWrapper) {
        return m_dbWrapper->searchDocuments(query, filters);
    }
    return QVariantList();
}

QVariantMap DataManager::getDocumentStatistics() const
{
    QVariantMap stats;

    if (m_dbWrapper) {
        QVariantMap dbStats = m_dbWrapper->getStatistics();
        stats["totalDocuments"] = dbStats.value("totalDocuments", 0);
        stats["totalChunks"] = dbStats.value("totalChunks", 0);

        // Calculate processed documents
        QVariantMap statusCounts = dbStats.value("statusCounts").toMap();
        int processed = 0;
        for (auto it = statusCounts.constBegin(); it != statusCounts.constEnd(); ++it) {
            int status = it.key().toInt();
            if (status >= static_cast<int>(ProcessingStatus::Parsed)) {
                processed += it.value().toInt();
            }
        }
        stats["processedDocuments"] = processed;
        stats["statusCounts"] = statusCounts;
    } else {
        stats["totalDocuments"] = 0;
        stats["totalChunks"] = 0;
        stats["processedDocuments"] = 0;
    }
    stats["failedDocuments"] = 0;

    return stats;
}

bool DataManager::verifyDataIntegrity() const
{
    qCDebug(dataManager) << "Verifying data integrity for project:" << m_workDir;

    bool isValid = true;

    // Check database integrity
    if (m_dbWrapper) {
        // Basic checks - in a real implementation, you would do more thorough validation
        QVariantList allDocs = m_dbWrapper->getAllDocuments();
        for (const QVariant &docVariant : allDocs) {
            QVariantMap doc = docVariant.toMap();
            QString documentId = doc.value("id").toString();
            QString filePath = doc.value("file_path").toString();

            // Check if file still exists
            if (!QFileInfo::exists(filePath)) {
                qCWarning(dataManager) << "Document file missing:" << filePath;
                isValid = false;
            }
        }
    }

    return isValid;
}

bool DataManager::updateProcessingStatus2Vector(const QStringList &documentIds)
{
    if (documentIds.isEmpty()) {
        return false;
    }
    bool result = true;
    for (const QString &id : documentIds) {
        result &= updateProcessingStatus(id, ProcessingStatus::Vectorized);
    }
    return result;
}

bool DataManager::isReadyForProcessing(const QString &documentId) const
{
    if (documentId.isEmpty()) {
        return false;
    }

    if (m_dbWrapper) {
        QVariantMap doc = m_dbWrapper->getDocument(documentId);
        int docStatus = doc.value("processing_status").toInt();
        if (docStatus == static_cast<int>(ProcessingStatus::Unprocessed)) {
            return true;
        }
    }
    return false;
}

QVariantMap DataManager::getChunkById(const QString &chunkId) const
{
    if (chunkId.isEmpty()) {
        return QVariantMap();
    }
    if (m_dbWrapper) {
        QVariantMap chunk = m_dbWrapper->getChunk(chunkId);
        return chunk;
    }
    return QVariantMap();
}

QString DataManager::getDocumentIdByPath(const QString &documentPath) const
{
    if (documentPath.isEmpty()) {
        return QString();
    }

    if (m_dbWrapper) {
        QString path = m_dbWrapper->getDocumentId(documentPath);
        return path;
    }
    return QString();
}

bool DataManager::validateDocumentPath(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    return fileInfo.exists() && fileInfo.isFile() && fileInfo.isReadable();
}

QString DataManager::generateDocumentId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

DataManager::DocumentType DataManager::detectDocumentType(const QString &filePath) const
{
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    QString mimeTypeName = mimeType.name();

    if (mimeTypeName.startsWith("text/")) {
        return DocumentType::Text;
    } else if (mimeTypeName.startsWith("image/")) {
        return DocumentType::Image;
    } else if (mimeTypeName.startsWith("audio/")) {
        return DocumentType::Audio;
    } else if (mimeTypeName.startsWith("video/")) {
        return DocumentType::Video;
    } else if (mimeTypeName == "application/pdf") {
        return DocumentType::PDF;
    } else if (mimeTypeName.contains("office") || mimeTypeName.contains("word") ||
               mimeTypeName.contains("excel") || mimeTypeName.contains("powerpoint")) {
        return DocumentType::Office;
    }

    return DocumentType::Text; // Default
}

QString DataManager::calculateFileHash(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);

    return hash.result().toHex();
}

bool DataManager::updateProcessingStatus(const QString &documentId, ProcessingStatus status)
{
    if (m_dbWrapper) {
        DocumentDbWrapper::ProcessingStatus dbStatus = static_cast<DocumentDbWrapper::ProcessingStatus>(status);
        return m_dbWrapper->updateProcessingStatus(documentId, dbStatus);
    }

    return true;
}

bool DataManager::initializeComponents()
{
    // Initialize database wrapper
    m_dbWrapper.reset(new DocumentDbWrapper(m_workDir, this));
    if (!m_dbWrapper->initialize()) {
        qCWarning(dataManager) << "Failed to initialize database wrapper";
        return false;
    }

    return true;
}

QString DataManager::getDocumentPathById(const QString &id)
{
    return getDocumentInfo(id).value("file_path").toString();
}

int DataManager::getDocumentNameCount(const QString &documentName) const
{
    if (documentName.isEmpty()) {
        return false;
    }

    if (m_dbWrapper) {
        return m_dbWrapper->documentNameCount(documentName);
    }

    qCWarning(dataManager) << "Database wrapper not initialized";
    return false;
}

QString DataManager::genBackupFileName(const QString &filePath) const
{
    if (filePath.isEmpty())
        return QString();

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString baseName = fileInfo.baseName();

    int count = getDocumentNameCount(baseName);
    if (count > 0) {
        QStringList parts = fileName.split(".");
        if (parts.size() > 1) {
            QString name = parts.mid(0, parts.size() - 1).join(".");
            QString extension = parts.last();
            fileName = name + "_" + QString::number(count) + "." + extension;
        } else {
            fileName = fileName + "_" + QString::number(count);
        }
    }

    return fileName;
}

QString DataManager::getDocumentStorageDir() const
{
    return m_workDir + QDir::separator() + "documents";
}

QString DataManager::backupDocument(const QString &filePath, const QString &backupName)
{
    QString fileDir = getDocumentStorageDir();
    // Ensure storage directory exists
    QDir dir(fileDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCWarning(dataManager) << "Failed to create storage directory:" << dir.absolutePath();
            return "";
        }
    }

    QString backupFilePath = fileDir + QDir::separator() + backupName;
    // Copy file
    if (QFile::copy(filePath, backupFilePath)) {
        auto permissions = QFile::permissions(backupFilePath);
        permissions &= ~(QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther);
        QFile::setPermissions(backupFilePath, permissions);
        return backupFilePath;
    }
    return "";
}

bool DataManager::removeDocumentFromStorage(const QString &documentId)
{
    QString docPath = getDocumentPathById(documentId);
    return QFile::remove(docPath);
}

bool DataManager::validateDocumentId(const QString &documentId) const
{
    return !documentId.isEmpty() && documentId.length() <= 64;
}

bool DataManager::validateChunkId(const QString &chunkId) const
{
    return !chunkId.isEmpty() && chunkId.length() <= 64;
}

bool DataManager::storeDocumentContent(const QString &documentId, const QString &parsedContent, const QVariantMap &parseMetadata)
{
    qCDebug(dataManager) << "Storing document content for:" << documentId;

    if (!validateDocumentId(documentId) || parsedContent.isEmpty()) {
        qCWarning(dataManager) << "Invalid parameters for storeDocumentContent";
        return false;
    }

    if (!m_dbWrapper) {
        qCWarning(dataManager) << "Database wrapper not initialized";
        return false;
    }

    // Update document with parsed content and metadata
    QVariantMap updateData = parseMetadata;
    updateData["parsed_content"] = parsedContent;
    updateData["content_length"] = parsedContent.length();
    
    // Update processing status to Parsed
    updateData["processing_status"] = static_cast<int>(ProcessingStatus::Parsed);

    bool success = m_dbWrapper->updateDocument(documentId, updateData);
    if (success) {
        // Update processing status
        updateProcessingStatus(documentId, ProcessingStatus::Parsed);
        
        emit documentDataStored(documentId, true);
        qCDebug(dataManager) << "Document content stored successfully for:" << documentId;
    } else {
        emit documentDataStored(documentId, false);
        qCWarning(dataManager) << "Failed to store document content for:" << documentId;
    }

    return success;
}

bool DataManager::storeDocumentChunks(const QString &documentId, const QVariantList &chunks)
{
    qCDebug(dataManager) << "Storing document chunks for:" << documentId << "chunks count:" << chunks.size();

    if (!validateDocumentId(documentId) || chunks.isEmpty()) {
        qCWarning(dataManager) << "Invalid parameters for storeDocumentChunks";
        return false;
    }

    if (!validateChunkData(chunks)) {
        qCWarning(dataManager) << "Invalid chunk data format";
        return false;
    }

    if (!m_dbWrapper) {
        qCWarning(dataManager) << "Database wrapper not initialized";
        return false;
    }

    // Store chunks in database
    bool success = m_dbWrapper->insertChunks(documentId, chunks);
    if (success) {
        // Update processing status to Chunked
        updateProcessingStatus(documentId, ProcessingStatus::Chunked);
        m_dbWrapper->updateProcessingStatus(documentId, DocumentDbWrapper::ProcessingStatus::Chunked);
        
        emit documentChunksStored(documentId, chunks.size());
        qCDebug(dataManager) << "Document chunks stored successfully for:" << documentId;
    } else {
        emit errorOccurred("storeDocumentChunks", QString("Failed to store chunks for document: %1").arg(documentId));
        qCWarning(dataManager) << "Failed to store document chunks for:" << documentId;
    }

    return success;
}

bool DataManager::validateStorageConfig(const QVariantMap &config) const
{
    // Basic validation for storage configuration
    if (config.isEmpty()) {
        return false;
    }
    
    // Check for required fields if any
    // This can be extended based on specific storage requirements
    return true;
}

bool DataManager::validateChunkData(const QVariantList &chunks) const
{
    if (chunks.isEmpty()) {
        return false;
    }
    
    for (const QVariant &chunkVariant : chunks) {
        QVariantMap chunk = chunkVariant.toMap();
        
        // Validate required fields
        if (!chunk.contains("id") || chunk.value("id").toString().isEmpty()) {
            qCWarning(dataManager) << "Chunk missing required 'id' field";
            return false;
        }
        
        if (!chunk.contains("content") || chunk.value("content").toString().isEmpty()) {
            qCWarning(dataManager) << "Chunk missing required 'content' field";
            return false;
        }
        
        // Validate chunk ID format
        QString chunkId = chunk.value("id").toString();
        if (!validateChunkId(chunkId)) {
            qCWarning(dataManager) << "Invalid chunk ID format:" << chunkId;
            return false;
        }
    }
    
    return true;
}

QString DataManager::calculateSHA256(const QString &filePath) const
{
    QString ret;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return ret;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        QByteArray result = hash.result();
        ret = result.toHex();
    }

    file.close();
    return ret;
}
