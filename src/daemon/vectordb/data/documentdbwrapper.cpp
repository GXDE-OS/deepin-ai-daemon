// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "documentdbwrapper.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(documentDbWrapper, "aidaemon.vectordb.documentdbwrapper")

DocumentDbWrapper::DocumentDbWrapper(const QString &projectDir, QObject *parent)
    : QObject(parent)
    , m_projectDir(projectDir)
    , m_connectionName(getConnectionName())
{
    qCDebug(documentDbWrapper) << "Creating document database wrapper for project:" << projectDir;
}

DocumentDbWrapper::~DocumentDbWrapper()
{
    if (m_database.isOpen()) {
        m_database.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DocumentDbWrapper::initialize()
{
    qCDebug(documentDbWrapper) << "Initializing document database for project:" << m_projectDir;

    // Create database directory if it doesn't exist
    QString dbPath = getDatabasePath();
    QDir dbDir = QFileInfo(dbPath).absoluteDir();
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            qCWarning(documentDbWrapper) << "Failed to create database directory:" << dbDir.absolutePath();
            return false;
        }
    }

    // Setup database connection
    m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_database.setDatabaseName(dbPath);

    if (!m_database.open()) {
        qCWarning(documentDbWrapper) << "Failed to open database:" << m_database.lastError().text();
        return false;
    }

    // Create tables
    if (!createTables()) {
        qCWarning(documentDbWrapper) << "Failed to create database tables";
        return false;
    }

    qCDebug(documentDbWrapper) << "Document database initialized successfully";
    return true;
}

bool DocumentDbWrapper::insertDocument(const QString &documentId, const QString &fileName, const QVariantMap &metadata)
{
    qCDebug(documentDbWrapper) << "Inserting document:" << documentId;

    QString sql = R"(
        INSERT INTO documents (id, file_name, metadata, processing_status, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    QJsonDocument metadataDoc = QJsonDocument::fromVariant(metadata);
    QDateTime now = QDateTime::currentDateTime();

    QVariantList params;
    params << documentId << fileName << metadataDoc.toJson(QJsonDocument::Compact)
           << static_cast<int>(ProcessingStatus::Unprocessed) << now << now;

    return executeQuery(sql, params);
}

bool DocumentDbWrapper::updateDocument(const QString &documentId, const QVariantMap &metadata)
{
    qCDebug(documentDbWrapper) << "Updating document:" << documentId;

    QString sql = R"(
        UPDATE documents 
        SET metadata = ?, updated_at = ?
        WHERE id = ?
    )";

    QJsonDocument metadataDoc = QJsonDocument::fromVariant(metadata);
    QDateTime now = QDateTime::currentDateTime();

    QVariantList params;
    params << metadataDoc.toJson(QJsonDocument::Compact) << now << documentId;

    return executeQuery(sql, params);
}

bool DocumentDbWrapper::removeDocument(const QString &documentId)
{
    qCDebug(documentDbWrapper) << "Removing document:" << documentId;

    // Remove chunks first
    QString deleteChunksSql = "DELETE FROM chunks WHERE document_id = ?";
    if (!executeQuery(deleteChunksSql, {documentId})) {
        qCWarning(documentDbWrapper) << "Failed to remove document chunks";
        return false;
    }

    // Remove document
    QString deleteDocSql = "DELETE FROM documents WHERE id = ?";
    return executeQuery(deleteDocSql, {documentId});
}

QVariantMap DocumentDbWrapper::getDocument(const QString &documentId) const
{
    QString sql = "SELECT * FROM documents WHERE id = ?";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(documentId);

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Failed to get document:" << query.lastError().text();
        return QVariantMap();
    }

    if (query.next()) {
        QVariantMap doc = queryToVariantMap(query);        
        QString docName = doc.value("file_name").toString();
        QString docPath = m_projectDir + QDir::separator() + "documents" + QDir::separator() + docName;
        doc.insert("file_path", QVariant::fromValue(docPath));

        // Parse metadata JSON
        QString metadataJson = doc.value("metadata").toString();
        if (!metadataJson.isEmpty()) {
            QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataJson.toUtf8());
            doc["metadata"] = metadataDoc.toVariant();
        }
        
        return doc;
    }

    return QVariantMap();
}

QVariantList DocumentDbWrapper::getAllDocuments() const
{
    QString sql = "SELECT * FROM documents ORDER BY created_at DESC";
    QSqlQuery query = prepareQuery(sql);

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Failed to get all documents:" << query.lastError().text();
        return QVariantList();
    }

    QVariantList documents;
    while (query.next()) {
        QVariantMap doc = queryToVariantMap(query);
        QString docName = doc.value("file_name").toString();
        QString docPath = m_projectDir + QDir::separator() + "documents" + QDir::separator() + docName;
        doc.insert("file_path", QVariant::fromValue(docPath));
        
        // Parse metadata JSON
        QString metadataJson = doc.value("metadata").toString();
        if (!metadataJson.isEmpty()) {
            QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataJson.toUtf8());
            doc["metadata"] = metadataDoc.toVariant();
        }
        
        documents.append(doc);
    }

    return documents;
}

QVariantList DocumentDbWrapper::getDocumentsByStatus(ProcessingStatus status) const
{
    QString sql = "SELECT * FROM documents WHERE processing_status = ? ORDER BY created_at DESC";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(static_cast<int>(status));

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Failed to get documents by status:" << query.lastError().text();
        return QVariantList();
    }

    QVariantList documents;
    while (query.next()) {
        QVariantMap doc = queryToVariantMap(query);
        
        // Parse metadata JSON
        QString metadataJson = doc.value("metadata").toString();
        if (!metadataJson.isEmpty()) {
            QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataJson.toUtf8());
            doc["metadata"] = metadataDoc.toVariant();
        }
        
        documents.append(doc);
    }

    return documents;
}

bool DocumentDbWrapper::updateProcessingStatus(const QString &documentId, ProcessingStatus status)
{
    qCDebug(documentDbWrapper) << "Updating processing status for document:" << documentId << "to:" << static_cast<int>(status);

    QString sql = R"(
        UPDATE documents 
        SET processing_status = ?, updated_at = ?
        WHERE id = ?
    )";

    QDateTime now = QDateTime::currentDateTime();
    QVariantList params;
    params << static_cast<int>(status) << now << documentId;

    return executeQuery(sql, params);
}

bool DocumentDbWrapper::insertChunks(const QString &documentId, const QVariantList &chunks)
{
    qCDebug(documentDbWrapper) << "Inserting" << chunks.size() << "chunks for document:" << documentId;

    if (chunks.isEmpty()) {
        qCWarning(documentDbWrapper) << "Empty chunks list provided";
        return true;
    }

    QString sql = R"(
        INSERT INTO chunks (id, document_id, content, metadata, chunk_index, created_at)
        VALUES (?, ?, ?, ?, ?, ?)
    )";

    QSqlQuery query = prepareQuery(sql);
    
    // Start transaction for batch operation
    if (!m_database.transaction()) {
        qCWarning(documentDbWrapper) << "Failed to start transaction:" << m_database.lastError().text();
        return false;
    }

    QDateTime now = QDateTime::currentDateTime();
    bool success = true;

    for (int i = 0; i < chunks.size(); ++i) {
        QVariantMap chunk = chunks[i].toMap();
        QString chunkId = chunk.value("id").toString();
        QString content = chunk.value("content").toString();
        QVariantMap metadata = chunk.value("metadata").toMap();

        QJsonDocument metadataDoc = QJsonDocument::fromVariant(metadata);

        query.addBindValue(chunkId);
        query.addBindValue(documentId);
        query.addBindValue(content);
        query.addBindValue(metadataDoc.toJson(QJsonDocument::Compact));
        query.addBindValue(i);
        query.addBindValue(now);

        if (!query.exec()) {
            qCWarning(documentDbWrapper) << "Failed to insert chunk:" << query.lastError().text();
            success = false;
            break;
        }
        
        // Reset the query for next iteration
        query.finish();
    }

    if (success) {
        if (!m_database.commit()) {
            qCWarning(documentDbWrapper) << "Failed to commit transaction:" << m_database.lastError().text();
            success = false;
        } else {
            qCDebug(documentDbWrapper) << "Successfully inserted" << chunks.size() << "chunks";
        }
    } else {
        if (!m_database.rollback()) {
            qCWarning(documentDbWrapper) << "Failed to rollback transaction:" << m_database.lastError().text();
        }
    }

    return success;
}

QVariantList DocumentDbWrapper::getChunks(const QString &documentId) const
{
    QString sql = "SELECT * FROM chunks WHERE document_id = ? ORDER BY chunk_index";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(documentId);

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Failed to get chunks:" << query.lastError().text();
        return QVariantList();
    }

    QVariantList chunks;
    while (query.next()) {
        QVariantMap chunk = queryToVariantMap(query);
        
        // Parse metadata JSON
        QString metadataJson = chunk.value("metadata").toString();
        if (!metadataJson.isEmpty()) {
            QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataJson.toUtf8());
            chunk["metadata"] = metadataDoc.toVariant();
        }
        
        chunks.append(chunk);
    }

    return chunks;
}

QVariantMap DocumentDbWrapper::getChunk(const QString &chunkId) const
{
    if (chunkId.isEmpty()) {
        return QVariantMap();
    }
    QString sql = "SELECT * FROM chunks WHERE id = ?";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(chunkId);

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Failed to get chunk:" << query.lastError().text();
        return QVariantMap();
    }

    if (query.next()) {
        return queryToVariantMap(query);
    }

    return QVariantMap();
}

bool DocumentDbWrapper::updateChunk(const QString &chunkId, const QString &content)
{
    qCDebug(documentDbWrapper) << "Updating chunk:" << chunkId;

    QString sql = "UPDATE chunks SET content = ? WHERE id = ?";
    QVariantList params;
    params << content << chunkId;

    return executeQuery(sql, params);
}

QVariantList DocumentDbWrapper::searchDocuments(const QString &query, const QVariantMap &filters) const
{
    Q_UNUSED(filters) // TODO: Implement filters

    QString sql = R"(
        SELECT * FROM documents 
        WHERE file_name LIKE ? OR metadata LIKE ?
        ORDER BY created_at DESC
    )";

    QString searchPattern = QString("%%1%").arg(query);
    QSqlQuery sqlQuery = prepareQuery(sql);
    sqlQuery.addBindValue(searchPattern);
    sqlQuery.addBindValue(searchPattern);

    if (!sqlQuery.exec()) {
        qCWarning(documentDbWrapper) << "Failed to search documents:" << sqlQuery.lastError().text();
        return QVariantList();
    }

    QVariantList documents;
    while (sqlQuery.next()) {
        QVariantMap doc = queryToVariantMap(sqlQuery);
        
        // Parse metadata JSON
        QString metadataJson = doc.value("metadata").toString();
        if (!metadataJson.isEmpty()) {
            QJsonDocument metadataDoc = QJsonDocument::fromJson(metadataJson.toUtf8());
            doc["metadata"] = metadataDoc.toVariant();
        }
        
        documents.append(doc);
    }

    return documents;
}

bool DocumentDbWrapper::updateDocumentIndexId(const QString &documentId, const QString &indexId)
{
    if (documentId.isEmpty()) {
        qCWarning(documentDbWrapper) << "Invalid document ID:" << documentId;
        return false;
    }
    
    QString sql = "UPDATE documents SET index_id = ? WHERE id = ?";
    QVariantList params;
    params << indexId << documentId;

    return executeQuery(sql, params);
}

QString DocumentDbWrapper::getDocumentIndexId(const QString &documentId)
{
    if (documentId.isEmpty()) {
        qCWarning(documentDbWrapper) << "Invalid document ID:" << documentId;
        return QString();
    }
    qCDebug(documentDbWrapper) << "Getting index ID for document:" << documentId;
    QString sql = "SELECT index_id FROM documents WHERE id = ?";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(documentId);

    if (query.exec() && query.next()) {
        return query.value("index_id").toString();
    }

    return QString();
}

QVariantMap DocumentDbWrapper::getStatistics() const
{
    QVariantMap stats;

    // Total documents
    QString totalSql = "SELECT COUNT(*) as total FROM documents";
    QSqlQuery totalQuery = prepareQuery(totalSql);
    if (totalQuery.exec() && totalQuery.next()) {
        stats["totalDocuments"] = totalQuery.value("total").toInt();
    }

    // Documents by status
    QString statusSql = "SELECT processing_status, COUNT(*) as count FROM documents GROUP BY processing_status";
    QSqlQuery statusQuery = prepareQuery(statusSql);
    if (statusQuery.exec()) {
        QVariantMap statusCounts;
        while (statusQuery.next()) {
            int status = statusQuery.value("processing_status").toInt();
            int count = statusQuery.value("count").toInt();
            statusCounts[QString::number(status)] = count;
        }
        stats["statusCounts"] = statusCounts;
    }

    // Total chunks
    QString chunksSql = "SELECT COUNT(*) as total FROM chunks";
    QSqlQuery chunksQuery = prepareQuery(chunksSql);
    if (chunksQuery.exec() && chunksQuery.next()) {
        stats["totalChunks"] = chunksQuery.value("total").toInt();
    }

    return stats;
}

bool DocumentDbWrapper::documentExists(const QString &documentId) const
{
    QString sql = "SELECT 1 FROM documents WHERE id = ? LIMIT 1";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(documentId);

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Failed to check document existence:" << query.lastError().text();
        return false;
    }

    return query.next();
}

int DocumentDbWrapper::documentNameCount(const QString &documentName) const
{
    QString sql = "SELECT COUNT(*) as count FROM documents WHERE file_name LIKE ?";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(QString("%%1%").arg(documentName));

    if (query.exec() && query.next()) {
        return query.value("count").toInt();
    }

    return 0;
}

QString DocumentDbWrapper::getDatabasePath() const
{
    return m_projectDir + QDir::separator() + QString("documents.db");
}

QString DocumentDbWrapper::getDocumentId(const QString &documentPath) const
{
    if (documentPath.isEmpty()) {
        qCWarning(documentDbWrapper) << "Invalid document path:" << documentPath;
        return QString();
    }

    qCDebug(documentDbWrapper) << "Getting document ID for document path:" << documentPath;
    QString sql = "SELECT id FROM documents WHERE file_name = ?";
    QSqlQuery query = prepareQuery(sql);
    query.addBindValue(documentPath);

    if (query.exec() && query.next()) {
        return query.value("id").toString();
    }

    return QString();
}

bool DocumentDbWrapper::createTables()
{
    // Documents table
    QString documentsTable = R"(
        CREATE TABLE IF NOT EXISTS documents (
            id TEXT PRIMARY KEY,
            file_name TEXT NOT NULL,
            index_id TEXT,
            metadata TEXT,
            processing_status INTEGER DEFAULT 0,
            created_at DATETIME NOT NULL,
            updated_at DATETIME NOT NULL
        )
    )";

    if (!executeQuery(documentsTable)) {
        qCWarning(documentDbWrapper) << "Failed to create documents table";
        return false;
    }

    // Chunks table
    QString chunksTable = R"(
        CREATE TABLE IF NOT EXISTS chunks (
            id TEXT PRIMARY KEY,
            document_id TEXT NOT NULL,
            content TEXT NOT NULL,
            metadata TEXT,
            chunk_index INTEGER NOT NULL,
            created_at DATETIME NOT NULL,
            FOREIGN KEY (document_id) REFERENCES documents (id) ON DELETE CASCADE
        )
    )";

    if (!executeQuery(chunksTable)) {
        qCWarning(documentDbWrapper) << "Failed to create chunks table";
        return false;
    }

    // Create indexes
    QString documentsIndex = "CREATE INDEX IF NOT EXISTS idx_documents_status ON documents (processing_status)";
    QString chunksIndex = "CREATE INDEX IF NOT EXISTS idx_chunks_document ON chunks (document_id)";

    if (!executeQuery(documentsIndex) || !executeQuery(chunksIndex)) {
        qCWarning(documentDbWrapper) << "Failed to create database indexes";
        return false;
    }

    return true;
}

bool DocumentDbWrapper::executeQuery(const QString &sql, const QVariantList &params) const
{
    QSqlQuery query = prepareQuery(sql);
    
    for (const QVariant &param : params) {
        query.addBindValue(param);
    }

    if (!query.exec()) {
        qCWarning(documentDbWrapper) << "Query execution failed:" << query.lastError().text();
        qCDebug(documentDbWrapper) << "SQL:" << sql;
        qCDebug(documentDbWrapper) << "Params:" << params;
        return false;
    }

    return true;
}

QSqlQuery DocumentDbWrapper::prepareQuery(const QString &sql) const
{
    QSqlQuery query(m_database);
    if (!query.prepare(sql)) {
        qCWarning(documentDbWrapper) << "Query preparation failed:" << query.lastError().text();
        qCDebug(documentDbWrapper) << "SQL:" << sql;
    }
    return query;
}

QString DocumentDbWrapper::getConnectionName() const
{
    return QString("DocumentDb_%1_%2").arg(m_projectDir).arg(reinterpret_cast<quintptr>(this));
}

QVariantMap DocumentDbWrapper::queryToVariantMap(const QSqlQuery &query) const
{
    QVariantMap map;
    QSqlRecord record = query.record();
    
    for (int i = 0; i < record.count(); ++i) {
        QString fieldName = record.fieldName(i);
        QVariant value = query.value(i);
        map[fieldName] = value;
    }
    
    return map;
}

QVariantList DocumentDbWrapper::queryToVariantList(const QSqlQuery &query) const
{
    QVariantList list;
    QSqlQuery q = query; // Make a copy since query methods are not const
    
    while (q.next()) {
        list.append(queryToVariantMap(q));
    }
    
    return list;
}

AIDAEMON_VECTORDB_END_NAMESPACE
