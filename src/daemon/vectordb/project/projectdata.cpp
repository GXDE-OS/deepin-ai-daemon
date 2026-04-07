// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "projectdata.h"

#include <QLoggingCategory>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(projectData, "vectordb.project.data")

ProjectData::ProjectData(QObject *parent)
    : QObject(parent)
{
    m_projectInfo.createdTime = QDateTime::currentDateTime();
    m_projectInfo.updatedTime = m_projectInfo.createdTime;
    m_projectInfo.lastAccessTime = m_projectInfo.createdTime;
}

ProjectData::~ProjectData()
{

}

void ProjectData::setName(const QString &name)
{
    if (m_projectInfo.name != name) {
        m_projectInfo.name = name;
        updateTimestamp();
    }
}

void ProjectData::setStatus(ProjectStatus status)
{
    if (m_projectInfo.status != status) {
        m_projectInfo.status = status;
        updateTimestamp();
        emit statusChanged();
    }
}

void ProjectData::setVectorModelId(const QString &modelId)
{
    if (m_projectInfo.vectorModelId != modelId) {
        m_projectInfo.vectorModelId = modelId;
        updateTimestamp();
    }
}

QVariantMap ProjectData::toVariantMap() const
{
    QVariantMap map;
    
    // Project info
    map["id"] = m_projectInfo.id;
    map["name"] = m_projectInfo.name;
    map["appId"] = m_projectInfo.appId;
    map["status"] = static_cast<int>(m_projectInfo.status);
    map["vectorModelId"] = m_projectInfo.vectorModelId;
    map["createdTime"] = m_projectInfo.createdTime;
    map["updatedTime"] = m_projectInfo.updatedTime;
    map["lastAccessTime"] = m_projectInfo.lastAccessTime;
    
    // Status info
    map["dataDirectory"] = m_statusInfo.dataDirectory;
    map["totalDocuments"] = m_statusInfo.totalDocuments;
    map["totalVectors"] = m_statusInfo.totalVectors;
    
    return map;
}

void ProjectData::fromVariantMap(const QVariantMap &map)
{
    // Project info
    m_projectInfo.id = map.value("id").toString();
    m_projectInfo.name = map.value("name").toString();
    m_projectInfo.appId = map.value("appId").toString();
    m_projectInfo.status = static_cast<ProjectStatus>(map.value("status", 1).toInt());
    m_projectInfo.vectorModelId = map.value("vectorModelId").toString();
    m_projectInfo.createdTime = map.value("createdTime").toDateTime();
    m_projectInfo.updatedTime = map.value("updatedTime").toDateTime();
    m_projectInfo.lastAccessTime = map.value("lastAccessTime").toDateTime();
    
    // Status info
    m_statusInfo.dataDirectory = map.value("dataDirectory").toString();
    m_statusInfo.totalDocuments = map.value("totalDocuments", 0).toInt();
    m_statusInfo.totalVectors = map.value("totalVectors", 0).toInt();
}

bool ProjectData::isValid() const
{
    return !m_projectInfo.id.isEmpty() && 
           !m_projectInfo.appId.isEmpty() && 
           !m_projectInfo.vectorModelId.isEmpty();
}

QStringList ProjectData::validate() const
{
    QStringList errors;
    
    if (m_projectInfo.id.isEmpty()) {
        errors.append("Project ID is empty");
    }
    
    if (m_projectInfo.appId.isEmpty()) {
        errors.append("App ID is empty");
    }
    
    if (m_projectInfo.vectorModelId.isEmpty()) {
        errors.append("Vector Model ID is empty");
    }
    
    return errors;
}

void ProjectData::updateTimestamp()
{
    m_projectInfo.updatedTime = QDateTime::currentDateTime();
}
