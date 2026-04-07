// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOCUMENTINFO_H
#define DOCUMENTINFO_H

#include "aidaemon_global.h"

#include <QtCore/QString>
#include <QtCore/QVariantMap>
#include <QtCore/QDateTime>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Document information container
 * 
 * Holds metadata and status information for documents
 * in the vector database system.
 */
class DocumentInfo
{
public:
    enum class ProcessingStatus {
        Unprocessed = 0,
        Parsed = 1,
        Chunked = 2,
        Vectorized = 3
    };

    enum class DocumentType {
        Text = 1,
        Image = 2,
        Audio = 3,
        Video = 4,
        PDF = 5,
        Office = 6
    };

    DocumentInfo() = default;
    DocumentInfo(const QString &id, const QString &filePath);
    
    // Getters
    QString getId() const { return m_id; }
    QString getFilePath() const { return m_filePath; }
    DocumentType getType() const { return m_type; }
    ProcessingStatus getStatus() const { return m_status; }
    QVariantMap getMetadata() const { return m_metadata; }
    QDateTime getCreatedAt() const { return m_createdAt; }
    QDateTime getUpdatedAt() const { return m_updatedAt; }
    qint64 getFileSize() const { return m_fileSize; }
    QString getFileHash() const { return m_fileHash; }
    
    // Setters
    void setId(const QString &id) { m_id = id; }
    void setFilePath(const QString &filePath) { m_filePath = filePath; }
    void setType(DocumentType type) { m_type = type; }
    void setStatus(ProcessingStatus status) { m_status = status; updateTimestamp(); }
    void setMetadata(const QVariantMap &metadata) { m_metadata = metadata; updateTimestamp(); }
    void setCreatedAt(const QDateTime &createdAt) { m_createdAt = createdAt; }
    void setUpdatedAt(const QDateTime &updatedAt) { m_updatedAt = updatedAt; }
    void setFileSize(qint64 fileSize) { m_fileSize = fileSize; }
    void setFileHash(const QString &fileHash) { m_fileHash = fileHash; }
    
    // Utility methods
    bool isValid() const { return !m_id.isEmpty() && !m_filePath.isEmpty(); }
    QVariantMap toVariantMap() const;
    void fromVariantMap(const QVariantMap &map);
    
private:
    QString m_id;
    QString m_filePath;
    DocumentType m_type = DocumentType::Text;
    ProcessingStatus m_status = ProcessingStatus::Unprocessed;
    QVariantMap m_metadata;
    QDateTime m_createdAt;
    QDateTime m_updatedAt;
    qint64 m_fileSize = 0;
    QString m_fileHash;
    
    void updateTimestamp() { m_updatedAt = QDateTime::currentDateTime(); }
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // DOCUMENTINFO_H
