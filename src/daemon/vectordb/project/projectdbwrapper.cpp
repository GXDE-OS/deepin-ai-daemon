// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "projectdbwrapper.h"
#include "projectdata.h"

#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(projectDbWrapper, "vectordb.project.db")

ProjectDbWrapper::ProjectDbWrapper(QObject *parent)
    : QObject(parent)
{
    qCDebug(projectDbWrapper) << "ProjectDbWrapper created";
}

ProjectDbWrapper::~ProjectDbWrapper()
{
    close();
    qCDebug(projectDbWrapper) << "ProjectDbWrapper destroyed";
}

bool ProjectDbWrapper::initialize(const QString &dbPath)
{
    qCDebug(projectDbWrapper) << "Initializing database with path:" << dbPath;
    
    if (m_isInitialized) {
        qCDebug(projectDbWrapper) << "Database already initialized";
        return true;
    }
    
    m_dbPath = dbPath.isEmpty() ? getDefaultDatabasePath() : dbPath;
    
    // Ensure database directory exists
    QDir dbDir = QFileInfo(m_dbPath).absoluteDir();
    if (!dbDir.exists()) {
        if (!dbDir.mkpath(".")) {
            qCWarning(projectDbWrapper) << "Failed to create database directory:" << dbDir.absolutePath();
            return false;
        }
    }
    
    // Create database connection
    m_database.reset(new QSqlDatabase(QSqlDatabase::addDatabase("QSQLITE", "vectordb_projects")));
    m_database->setDatabaseName(m_dbPath);
    
    if (!m_database->open()) {
        qCWarning(projectDbWrapper) << "Failed to open database:" << m_database->lastError().text();
        return false;
    }
    
    // Create tables if they don't exist
    if (!createTables()) {
        qCWarning(projectDbWrapper) << "Failed to create database tables";
        return false;
    }
    
    m_isInitialized = true;
    qCDebug(projectDbWrapper) << "Database initialized successfully";
    return true;
}

void ProjectDbWrapper::close()
{
    if (m_database && m_database->isOpen()) {
        m_database->close();
        qCDebug(projectDbWrapper) << "Database closed";
    }
    m_isInitialized = false;
}

bool ProjectDbWrapper::insertProject(const ProjectData &project)
{
    qCDebug(projectDbWrapper) << "Inserting project:" << project.getId();
    
    if (!m_isInitialized || !validateProjectData(project)) {
        return false;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getInsertProjectQuery());
    
    // Bind project data
    query.bindValue(":project_id", project.getId());
    query.bindValue(":project_name", project.getName());
    query.bindValue(":app_id", project.getAppId());
    query.bindValue(":status", static_cast<int>(project.getStatus()));
    query.bindValue(":vector_model_id", project.getVectorModelId());
    query.bindValue(":created_time", project.getCreatedTime());
    query.bindValue(":updated_time", project.getUpdatedTime());
    query.bindValue(":last_access_time", project.getLastAccessTime());
    query.bindValue(":data_directory", project.getDir());
    query.bindValue(":total_documents", project.getTotalDocuments());
    query.bindValue(":total_vectors", project.getTotalVectors());
    
    if (!query.exec()) {
        logDatabaseError("insertProject");
        return false;
    }
    
    qCDebug(projectDbWrapper) << "Project inserted successfully:" << project.getId();
    return true;
}

bool ProjectDbWrapper::updateProject(const ProjectData &project)
{
    qCDebug(projectDbWrapper) << "Updating project:" << project.getId();
    
    if (!m_isInitialized || !validateProjectData(project)) {
        return false;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getUpdateProjectQuery());
    
    // Bind updated project data
    query.bindValue(":project_id", project.getId());
    query.bindValue(":project_name", project.getName());
    query.bindValue(":status", static_cast<int>(project.getStatus()));
    query.bindValue(":updated_time", project.getUpdatedTime());
    query.bindValue(":last_access_time", project.getLastAccessTime());
    query.bindValue(":data_directory", project.getDir());
    query.bindValue(":total_documents", project.getTotalDocuments());
    query.bindValue(":total_vectors", project.getTotalVectors());
    
    if (!query.exec()) {
        logDatabaseError("updateProject");
        return false;
    }
    
    qCDebug(projectDbWrapper) << "Project updated successfully:" << project.getId();
    return true;
}

bool ProjectDbWrapper::deleteProject(const QString &projectId)
{
    qCDebug(projectDbWrapper) << "Deleting project:" << projectId;
    
    if (!m_isInitialized || projectId.isEmpty()) {
        return false;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getDeleteProjectQuery());
    query.bindValue(":project_id", projectId);
    
    if (!query.exec()) {
        logDatabaseError("deleteProject");
        return false;
    }
    
    qCDebug(projectDbWrapper) << "Project deleted successfully:" << projectId;
    return true;
}

QSharedPointer<ProjectData> ProjectDbWrapper::getProject(const QString &projectId)
{
    if (!m_isInitialized || projectId.isEmpty()) {
        return nullptr;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getSelectProjectQuery());
    query.bindValue(":project_id", projectId);
    
    if (!query.exec()) {
        logDatabaseError("getProject");
        return nullptr;
    }
    
    if (query.next()) {
        return mapRowToProject(query);
    }
    
    return nullptr;
}

QList<QSharedPointer<ProjectData>> ProjectDbWrapper::getProjectsByAppId(const QString &appId)
{
    QList<QSharedPointer<ProjectData>> projects;
    
    if (!m_isInitialized || appId.isEmpty()) {
        return projects;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getSelectProjectsByAppIdQuery());
    query.bindValue(":app_id", appId);
    
    if (!query.exec()) {
        logDatabaseError("getProjectsByAppId");
        return projects;
    }
    
    while (query.next()) {
        auto project = mapRowToProject(query);
        if (project) {
            projects.append(project);
        }
    }
    
    return projects;
}

QList<QSharedPointer<ProjectData>> ProjectDbWrapper::getAllProjects()
{
    QList<QSharedPointer<ProjectData>> projects;
    
    if (!m_isInitialized) {
        return projects;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getSelectAllProjectsQuery());
    
    if (!query.exec()) {
        logDatabaseError("getAllProjects");
        return projects;
    }
    
    while (query.next()) {
        auto project = mapRowToProject(query);
        if (project) {
            projects.append(project);
        }
    }
    
    return projects;
}

bool ProjectDbWrapper::projectExists(const QString &projectId) const
{
    if (!m_isInitialized || projectId.isEmpty()) {
        return false;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getProjectExistsQuery());
    query.bindValue(":project_id", projectId);
    
    if (!query.exec()) {
        return false;
    }
    
    return query.next();
}

int ProjectDbWrapper::getProjectCount(const QString &appId) const
{
    if (!m_isInitialized) {
        return 0;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getProjectCountQuery());
    
    if (!appId.isEmpty()) {
        query.bindValue(":app_id", appId);
    }
    
    if (!query.exec() || !query.next()) {
        return 0;
    }
    
    return query.value(0).toInt();
}

QStringList ProjectDbWrapper::getProjectIds(const QString &appId) const
{
    QStringList projectIds;
    
    if (!m_isInitialized) {
        return projectIds;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getProjectIdsQuery());
    
    if (!appId.isEmpty()) {
        query.bindValue(":app_id", appId);
    }
    
    if (!query.exec()) {
        return projectIds;
    }
    
    while (query.next()) {
        projectIds.append(query.value(0).toString());
    }
    
    return projectIds;
}

bool ProjectDbWrapper::updateProjectStats(const QString &projectId, const QVariantMap &stats)
{
    qCDebug(projectDbWrapper) << "Updating project stats:" << projectId;
    
    if (!m_isInitialized || projectId.isEmpty()) {
        return false;
    }
    
    QSqlQuery query(*m_database);
    query.prepare(getUpdateStatsQuery());
    
    // Bind stats data (only fields available in simplified table)
    query.bindValue(":project_id", projectId);
    query.bindValue(":total_documents", stats.value("totalDocuments", 0).toInt());
    query.bindValue(":total_vectors", stats.value("totalVectors", 0).toInt());
    query.bindValue(":updated_time", QDateTime::currentDateTime());
    
    if (!query.exec()) {
        logDatabaseError("updateProjectStats");
        return false;
    }
    
    qCDebug(projectDbWrapper) << "Project stats updated successfully:" << projectId;
    return true;
}

bool ProjectDbWrapper::cleanupDeletedProjects()
{
    qCDebug(projectDbWrapper) << "Cleaning up deleted projects";
    
    if (!m_isInitialized) {
        return false;
    }
    
    // TODO: Implement cleanup deleted projects logic
    return false;
}

bool ProjectDbWrapper::vacuum()
{
    qCDebug(projectDbWrapper) << "Vacuuming database";
    
    if (!m_isInitialized) {
        return false;
    }
    
    QSqlQuery query(*m_database);
    return query.exec("VACUUM");
}

bool ProjectDbWrapper::createTables()
{
    QSqlQuery query(*m_database);
    return query.exec(getCreateTableQuery());
}

bool ProjectDbWrapper::executeQuery(const QString &queryStr, const QVariantMap &params)
{
    QSqlQuery query(*m_database);
    query.prepare(queryStr);
    
    for (auto it = params.begin(); it != params.end(); ++it) {
        query.bindValue(it.key(), it.value());
    }
    
    return query.exec();
}

QSharedPointer<ProjectData> ProjectDbWrapper::mapRowToProject(const QSqlQuery &query)
{
    auto project = QSharedPointer<ProjectData>::create();
    
    // Map basic project information from database
    project->setId(query.value("project_id").toString());
    project->setName(query.value("project_name").toString());
    project->setAppId(query.value("app_id").toString());
    project->setStatus(static_cast<ProjectData::ProjectStatus>(query.value("status").toInt()));
    project->setCreatedTime(query.value("created_time").toDateTime());
    project->setUpdatedTime(query.value("updated_time").toDateTime());
    project->setLastAccessTime(query.value("last_access_time").toDateTime());
    project->setVectorModelId(query.value("vector_model_id").toString());
    
    // Map status information
    project->setDir(query.value("data_directory").toString());
    project->setTotalDocuments(query.value("total_documents").toInt());
    project->setTotalVectors(query.value("total_vectors").toInt());
    
    return project;
}

QString ProjectDbWrapper::getDefaultDatabasePath() const
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QString("%1/vectordb/projects.db").arg(dataDir);
}

bool ProjectDbWrapper::validateProjectData(const ProjectData &project) const
{
    return project.isValid();
}

void ProjectDbWrapper::logDatabaseError(const QString &operation) const
{
    if (m_database) {
        qCWarning(projectDbWrapper) << "Database error in" << operation << ":" << m_database->lastError().text();
        emit const_cast<ProjectDbWrapper*>(this)->databaseError(m_database->lastError().text());
    }
}

QString ProjectDbWrapper::getInsertProjectQuery() const
{
    return R"(
        INSERT INTO projects (
            project_id, project_name, app_id, status,
            vector_model_id,
            created_time, updated_time, last_access_time,
            data_directory,
            total_documents, total_vectors
        ) VALUES (
            :project_id, :project_name, :app_id, :status,
            :vector_model_id,
            :created_time, :updated_time, :last_access_time,
            :data_directory, :total_documents, :total_vectors
        )
    )";
}

QString ProjectDbWrapper::getUpdateProjectQuery() const
{
    return R"(
        UPDATE projects SET
            project_name = :project_name,
            status = :status,
            updated_time = :updated_time,
            last_access_time = :last_access_time,
            data_directory = :data_directory,
            total_documents = :total_documents,
            total_vectors = :total_vectors
        WHERE project_id = :project_id
    )";
}

QString ProjectDbWrapper::getSelectProjectQuery() const
{
    return "SELECT * FROM projects WHERE project_id = :project_id";
}

QString ProjectDbWrapper::getSelectProjectsByAppIdQuery() const
{
    return "SELECT * FROM projects WHERE app_id = :app_id AND status != 3 ORDER BY created_time DESC";
}

QString ProjectDbWrapper::getSelectAllProjectsQuery() const
{
    return "SELECT * FROM projects WHERE status != 3 ORDER BY created_time DESC";
}

QString ProjectDbWrapper::getDeleteProjectQuery() const
{
    return "DELETE FROM projects WHERE project_id = :project_id";
}

QString ProjectDbWrapper::getProjectExistsQuery() const
{
    return "SELECT 1 FROM projects WHERE project_id = :project_id";
}

QString ProjectDbWrapper::getProjectCountQuery() const
{
    return "SELECT COUNT(*) FROM projects WHERE status != 3";
}

QString ProjectDbWrapper::getProjectIdsQuery() const
{
    return "SELECT project_id FROM projects WHERE status != 3 ORDER BY created_time DESC";
}

QString ProjectDbWrapper::getUpdateStatsQuery() const
{
    return R"(
        UPDATE projects SET
            data_directory = :data_directory,
            total_documents = :total_documents,
            total_vectors = :total_vectors,
            updated_time = :updated_time
        WHERE project_id = :project_id
    )";
}

QString ProjectDbWrapper::getCreateTableQuery() const
{
    return R"(
        CREATE TABLE IF NOT EXISTS projects (
            project_id VARCHAR(64) PRIMARY KEY,
            project_name VARCHAR(255) NOT NULL,
            app_id VARCHAR(128) NOT NULL,
            status INTEGER DEFAULT 1,
            
            vector_model_id VARCHAR(64) NOT NULL,
            
            created_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_access_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            
            data_directory VARCHAR(512),
            total_documents INTEGER DEFAULT 0,
            total_vectors INTEGER DEFAULT 0
        );
    )";
}
