// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PROJECTDBWRAPPER_H
#define PROJECTDBWRAPPER_H

#include "aidaemon_global.h"

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QVariantMap>
#include <QStringList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class ProjectData;

/**
 * @brief Database wrapper for project management operations
 * 
 * Handles database operations for project management including
 * CRUD operations, queries, and maintenance tasks.
 */
class ProjectDbWrapper : public QObject
{
    Q_OBJECT

public:
    explicit ProjectDbWrapper(QObject *parent = nullptr);
    ~ProjectDbWrapper();

    /**
     * @brief Initialize the database connection
     * @param dbDir Database dir path (optional, uses default if empty)
     * @return true on success, false on failure
     */
    bool initialize(const QString &dbPath = QString());

    /**
     * @brief Close the database connection
     */
    void close();

    /**
     * @brief Insert a new project
     * @param project Project data to insert
     * @return true on success, false on failure
     */
    bool insertProject(const ProjectData &project);

    /**
     * @brief Update an existing project
     * @param project Project data to update
     * @return true on success, false on failure
     */
    bool updateProject(const ProjectData &project);

    /**
     * @brief Delete a project
     * @param projectId Project identifier
     * @return true on success, false on failure
     */
    bool deleteProject(const QString &projectId);

    /**
     * @brief Get a project by ID
     * @param projectId Project identifier
     * @return Project data, nullptr if not found
     */
    QSharedPointer<ProjectData> getProject(const QString &projectId);

    /**
     * @brief Get all projects for an application
     * @param appId Application identifier
     * @return List of project data
     */
    QList<QSharedPointer<ProjectData>> getProjectsByAppId(const QString &appId);

    /**
     * @brief Get all projects
     * @return List of all project data
     */
    QList<QSharedPointer<ProjectData>> getAllProjects();

    /**
     * @brief Check if project exists
     * @param projectId Project identifier
     * @return true if exists, false otherwise
     */
    bool projectExists(const QString &projectId) const;

    /**
     * @brief Get project count
     * @param appId Application identifier (optional, all projects if empty)
     * @return Number of projects
     */
    int getProjectCount(const QString &appId = QString()) const;

    /**
     * @brief Get project IDs
     * @param appId Application identifier (optional, all projects if empty)
     * @return List of project IDs
     */
    QStringList getProjectIds(const QString &appId = QString()) const;

    /**
     * @brief Update project statistics
     * @param projectId Project identifier
     * @param stats Statistics to update
     * @return true on success, false on failure
     */
    bool updateProjectStats(const QString &projectId, const QVariantMap &stats);

    /**
     * @brief Clean up deleted projects
     * @return true on success, false on failure
     */
    bool cleanupDeletedProjects();

    /**
     * @brief Vacuum the database
     * @return true on success, false on failure
     */
    bool vacuum();

    /**
     * @brief Check if database is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return m_isInitialized; }

Q_SIGNALS:
    /**
     * @brief Emitted when a database error occurs
     * @param error Error message
     */
    void databaseError(const QString &error);

private:
    QScopedPointer<QSqlDatabase> m_database;
    QString m_dbPath;
    bool m_isInitialized = false;
    
    // Helper methods
    bool createTables();
    bool executeQuery(const QString &query, const QVariantMap &params = QVariantMap());
    QSharedPointer<ProjectData> mapRowToProject(const QSqlQuery &query);
    QString getDefaultDatabasePath() const;
    bool validateProjectData(const ProjectData &project) const;
    void logDatabaseError(const QString &operation) const;
    
    // SQL queries
    QString getInsertProjectQuery() const;
    QString getUpdateProjectQuery() const;
    QString getSelectProjectQuery() const;
    QString getSelectProjectsByAppIdQuery() const;
    QString getSelectAllProjectsQuery() const;
    QString getDeleteProjectQuery() const;
    QString getProjectExistsQuery() const;
    QString getProjectCountQuery() const;
    QString getProjectIdsQuery() const;
    QString getUpdateStatsQuery() const;
    QString getCreateTableQuery() const;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // PROJECTDBWRAPPER_H
