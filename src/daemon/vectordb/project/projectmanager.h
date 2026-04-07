// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include "aidaemon_global.h"

#include <QObject>
#include <QList>
#include <QScopedPointer>
#include <QHash>
#include <QSharedPointer>
#include <QVariantMap>
#include <QVariantList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class ProjectDbWrapper;
class ProjectData;

/**
 * @brief Project manager for handling vector database projects
 * 
 * Manages the lifecycle and configuration of vector database projects
 * for a specific application.
 */
class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(const QString &appId);
    ~ProjectManager();

    /**
     * @brief Initialize the project manager
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Create a new project
     * @param modelId Vector model identifier
     * @param config Project configuration
     * @return Project ID on success, empty string on failure
     */
    QString createProject(const QString &modelId, const QVariantMap &config);

    /**
     * @brief Delete a project
     * @param projectId Project identifier
     * @return true on success, false on failure
     */
    bool deleteProject(const QString &projectId);

    /**
     * @brief Update project configuration
     * @param projectId Project identifier
     * @param config New configuration
     * @return true on success, false on failure
     */
    bool updateProjectConfig(const QString &projectId, const QVariantMap &config);

    /**
     * @brief Get project information
     * @param projectId Project identifier
     * @return Project information as QVariantMap
     */
    QVariantMap getProjectInfo(const QString &projectId) const;

    /**
     * @brief Get all projects for this application
     * @return List of projects as QVariantList
     */
    QVariantList getAllProjects() const;

    /**
     * @brief Check if project exists
     * @param projectId Project identifier
     * @return true if exists, false otherwise
     */
    bool hasProject(const QString &projectId) const;

    /**
     * @brief Find project by model ID
     * @param modelId Vector model identifier
     * @return Project ID if found, empty string otherwise
     */
    QString findProjectByModelId(const QString &modelId) const;

    /**
     * @brief Create or get existing project for the given model
     * @param modelId Vector model identifier
     * @param config Project configuration (used only if creating new project)
     * @return Project ID on success, empty string on failure
     */
    QString createOrGetProject(const QString &modelId, const QVariantMap &config);

    /**
     * @brief Get project status
     * @param projectId Project identifier
     * @return Project status
     */
    int getProjectStatus(const QString &projectId) const;

    /**
     * @brief Get project statistics
     * @param projectId Project identifier
     * @return Project statistics as QVariantMap
     */
    QVariantMap getProjectStats(const QString &projectId) const;

    /**
     * @brief Get application ID
     * @return Application identifier
     */
    QString getAppId() const { return m_appId; }

    /**
     * @brief Get project count
     * @return Number of projects
     */
    int getProjectCount() const;

    /**
     * @brief Get project IDs
     * @return List of project IDs
     */
    QStringList getProjectIds() const;

public Q_SLOTS:
    /**
     * @brief Handle project update notification
     * @param projectId Project identifier
     */
    void onProjectUpdated(const QString &projectId);

    /**
     * @brief Refresh project cache
     */
    void refreshProjectCache();

Q_SIGNALS:
    /**
     * @brief Emitted when a project is created
     * @param projectId Project identifier
     */
    void projectCreated(const QString &projectId);

    /**
     * @brief Emitted when a project is deleted
     * @param projectId Project identifier
     */
    void projectDeleted(const QString &projectId);

    /**
     * @brief Emitted when a project is updated
     * @param projectId Project identifier
     */
    void projectUpdated(const QString &projectId);

    /**
     * @brief Emitted when an error occurs
     * @param error Error message
     */
    void errorOccurred(const QString &error);

private:
    QString m_appId;
    QList<QString> m_projectIds;
    QScopedPointer<ProjectDbWrapper> m_dbWrapper;
    QHash<QString, QSharedPointer<ProjectData>> m_projectCache;
    bool m_isInitialized = false;
    QString m_projectPath;
    
    // Helper methods
    QString generateProjectId() const;
    bool createProjectDirectory(const QString &projectId);
    bool removeProjectDirectory(const QString &projectId);
    void loadProjectsFromDatabase();
    void updateProjectCache(const QString &projectId);
    bool validateProjectConfig(const QVariantMap &config) const;
    QString getProjectDirectory(const QString &projectId) const;
    QString getRelativeProjectPath(const QString &projectId) const;
    QSharedPointer<ProjectData> createProjectData(const QString &projectId, 
                                                  const QString &modelId, 
                                                  const QVariantMap &config);
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // PROJECTMANAGER_H
