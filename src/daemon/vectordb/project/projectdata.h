// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROJECTDATA_H
#define PROJECTDATA_H

#include "aidaemon_global.h"

#include <QObject>
#include <QDateTime>
#include <QVariantMap>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Simplified project data structure matching database schema
 * 
 * Encapsulates core project information stored in database with Qt's
 * property system for meta-object access. Only contains essential fields.
 */
class ProjectData : public QObject
{
    Q_OBJECT
    
    // Q_PROPERTY declarations for database fields
    Q_PROPERTY(QString id READ getId CONSTANT)
    Q_PROPERTY(QString dir READ getDir CONSTANT)
    Q_PROPERTY(QString name READ getName WRITE setName)
    Q_PROPERTY(QString appId READ getAppId CONSTANT)
    Q_PROPERTY(ProjectStatus status READ getStatus WRITE setStatus NOTIFY statusChanged)
    Q_PROPERTY(QString vectorModelId READ getVectorModelId WRITE setVectorModelId)

public:
    enum class ProjectStatus {
        Active = 1,
        Paused = 2,
        Deleted = 3,
        Building = 4,
        Error = 5
    };
    Q_ENUM(ProjectStatus)

    explicit ProjectData(QObject *parent = nullptr);
    ~ProjectData();

    // Core project getters/setters
    QString getId() const { return m_projectInfo.id; }
    void setId(const QString &id) { m_projectInfo.id = id; }

    QString getName() const { return m_projectInfo.name; }
    void setName(const QString &name);
    
    QString getAppId() const { return m_projectInfo.appId; }
    void setAppId(const QString &appId) { m_projectInfo.appId = appId; }
    
    ProjectStatus getStatus() const { return m_projectInfo.status; }
    void setStatus(ProjectStatus status);
    
    QString getVectorModelId() const { return m_projectInfo.vectorModelId; }
    void setVectorModelId(const QString &modelId);
    
    QDateTime getCreatedTime() const { return m_projectInfo.createdTime; }
    void setCreatedTime(const QDateTime &time) { m_projectInfo.createdTime = time; }
    
    QDateTime getUpdatedTime() const { return m_projectInfo.updatedTime; }
    void setUpdatedTime(const QDateTime &time) { m_projectInfo.updatedTime = time; }
    
    QDateTime getLastAccessTime() const { return m_projectInfo.lastAccessTime; }
    void setLastAccessTime(const QDateTime &time) { m_projectInfo.lastAccessTime = time; }

    QString getDir() const { return m_statusInfo.dataDirectory; }
    void setDir(const QString &dir) { m_statusInfo.dataDirectory = dir; }
    
    int getTotalDocuments() const { return m_statusInfo.totalDocuments; }
    void setTotalDocuments(int count) { m_statusInfo.totalDocuments = count; }
    
    int getTotalVectors() const { return m_statusInfo.totalVectors; }
    void setTotalVectors(int count) { m_statusInfo.totalVectors = count; }
    
    // Serialization
    QVariantMap toVariantMap() const;
    void fromVariantMap(const QVariantMap &map);
    
    // Validation
    bool isValid() const;
    QStringList validate() const;

Q_SIGNALS:
    void statusChanged();
    void statisticsUpdated();

private:
    // Core project information matching database schema
    struct ProjectInfo {
        QString id;                    // project_id
        QString name;                  // project_name
        QString appId;                 // app_id
        ProjectStatus status = ProjectStatus::Active;  // status
        QString vectorModelId;         // vector_model_id
        QDateTime createdTime;         // created_time
        QDateTime updatedTime;         // updated_time
        QDateTime lastAccessTime;      // last_access_time
    } m_projectInfo;
    
    // Project statistics and data information
    struct StatusInfo {
        QString dataDirectory;         // data_directory
        int totalDocuments = 0;        // total_documents
        int totalVectors = 0;          // total_vectors
    } m_statusInfo;
    
    void updateTimestamp();
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // PROJECTDATA_H
