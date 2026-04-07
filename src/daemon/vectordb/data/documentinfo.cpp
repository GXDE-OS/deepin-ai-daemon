// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "documentinfo.h"

#include <QtCore/QFileInfo>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

DocumentInfo::DocumentInfo(const QString &id, const QString &filePath)
    : m_id(id)
    , m_filePath(filePath)
    , m_createdAt(QDateTime::currentDateTime())
    , m_updatedAt(QDateTime::currentDateTime())
{
    // Extract basic file information
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        m_fileSize = fileInfo.size();
        m_createdAt = fileInfo.birthTime();
        m_updatedAt = fileInfo.lastModified();
        
        // Determine document type from extension
        QString extension = fileInfo.suffix().toLower();
        if (extension == "txt" || extension == "md" || extension == "rst") {
            m_type = DocumentType::Text;
        } else if (extension == "pdf") {
            m_type = DocumentType::PDF;
        } else if (extension == "doc" || extension == "docx" || extension == "odt") {
            m_type = DocumentType::Office;
        } else if (extension == "jpg" || extension == "jpeg" || extension == "png" || extension == "gif") {
            m_type = DocumentType::Image;
        } else if (extension == "mp3" || extension == "wav" || extension == "flac") {
            m_type = DocumentType::Audio;
        } else if (extension == "mp4" || extension == "avi" || extension == "mkv") {
            m_type = DocumentType::Video;
        }
    }
}

QVariantMap DocumentInfo::toVariantMap() const
{
    QVariantMap map;
    map["id"] = m_id;
    map["filePath"] = m_filePath;
    map["type"] = static_cast<int>(m_type);
    map["status"] = static_cast<int>(m_status);
    map["metadata"] = m_metadata;
    map["createdAt"] = m_createdAt;
    map["updatedAt"] = m_updatedAt;
    map["fileSize"] = m_fileSize;
    map["fileHash"] = m_fileHash;
    
    return map;
}

void DocumentInfo::fromVariantMap(const QVariantMap &map)
{
    m_id = map.value("id").toString();
    m_filePath = map.value("filePath").toString();
    m_type = static_cast<DocumentType>(map.value("type", static_cast<int>(DocumentType::Text)).toInt());
    m_status = static_cast<ProcessingStatus>(map.value("status", static_cast<int>(ProcessingStatus::Unprocessed)).toInt());
    m_metadata = map.value("metadata").toMap();
    m_createdAt = map.value("createdAt").toDateTime();
    m_updatedAt = map.value("updatedAt").toDateTime();
    m_fileSize = map.value("fileSize").toLongLong();
    m_fileHash = map.value("fileHash").toString();
}

AIDAEMON_VECTORDB_END_NAMESPACE
