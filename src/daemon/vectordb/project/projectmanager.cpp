// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "projectmanager.h"
#include "projectdata.h"
#include "projectdbwrapper.h"

#include <QLoggingCategory>
#include <QUuid>
#include <QDir>
#include <QStandardPaths>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(projectManager, "vectordb.project.manager")

ProjectManager::ProjectManager(const QString &appId)
    : QObject(nullptr)
    , m_appId(appId)
    , m_dbWrapper(new ProjectDbWrapper(this))
{
    m_projectPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/vectordb/" + appId;
    qCDebug(projectManager) << "ProjectManager created for appId:" << appId;
}

ProjectManager::~ProjectManager()
{
    qCDebug(projectManager) << "ProjectManager destroyed for appId:" << m_appId;
}

bool ProjectManager::initialize()
{
    qCDebug(projectManager) << "Initializing ProjectManager for appId:" << m_appId;
    
    if (m_isInitialized) {
        qCDebug(projectManager) << "ProjectManager already initialized for appId:" << m_appId;
        return true;
    }

    // Create project directory if it doesn't exist
    QDir dir(m_projectPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qCWarning(projectManager) << "Failed to create project directory:" << dir.absolutePath();
            return false;
        }
    }
    
    // Initialize database wrapper
    if (!m_dbWrapper->initialize(m_projectPath + "/project.db")) {
        qCWarning(projectManager) << "Failed to initialize database wrapper for appId:" << m_appId;
        return false;
    }
    
    // Load existing projects from database
    loadProjectsFromDatabase();
    
    // Connect database wrapper signals
    connect(m_dbWrapper.data(), &ProjectDbWrapper::databaseError,
            this, &ProjectManager::errorOccurred);
    
    m_isInitialized = true;
    qCDebug(projectManager) << "ProjectManager initialized successfully for appId:" << m_appId;
    return true;
}

QString ProjectManager::createProject(const QString &modelId, const QVariantMap &config)
{
    qCDebug(projectManager) << "Creating project with modelId:" << modelId << "for appId:" << m_appId;
    
    if (!m_isInitialized) {
        qCWarning(projectManager) << "ProjectManager not initialized for appId:" << m_appId;
        return QString();
    }
    
    if (!validateProjectConfig(config)) {
        qCWarning(projectManager) << "Invalid parameters for createProject";
        return QString();
    }
    
    // Generate unique project ID
    QString projectId = generateProjectId();
    if (projectId.isEmpty()) {
        qCWarning(projectManager) << "Failed to generate project ID for appId:" << m_appId;
        return QString();
    }
    
    // Create project directory
    if (!createProjectDirectory(projectId)) {
        qCWarning(projectManager) << "Failed to create project directory for projectId:" << projectId;
        return QString();
    }
    
    // Create project data
    QSharedPointer<ProjectData> projectData = createProjectData(projectId, modelId, config);
    if (!projectData) {
        qCWarning(projectManager) << "Failed to create project data for projectId:" << projectId;
        removeProjectDirectory(projectId);
        return QString();
    }
    
    // Insert project into database
    if (!m_dbWrapper->insertProject(*projectData)) {
        qCWarning(projectManager) << "Failed to insert project into database for projectId:" << projectId;
        removeProjectDirectory(projectId);
        return QString();
    }
    
    // Add to cache and project list
    m_projectCache[projectId] = projectData;
    m_projectIds.append(projectId);
    
    emit projectCreated(projectId);
    qCDebug(projectManager) << "Project created successfully:" << projectId;
    return projectId;
}

bool ProjectManager::deleteProject(const QString &projectId)
{
    qCDebug(projectManager) << "Deleting project:" << projectId << "for appId:" << m_appId;
    
    if (!hasProject(projectId)) {
        qCWarning(projectManager) << "Invalid or non-existent projectId:" << projectId;
        return false;
    }
    
    // Delete from database
    if (!m_dbWrapper->deleteProject(projectId)) {
        qCWarning(projectManager) << "Failed to delete project from database:" << projectId;
        return false;
    }
    
    // Remove project directory
    removeProjectDirectory(projectId);
    
    // Remove from cache and project list
    m_projectCache.remove(projectId);
    m_projectIds.removeAll(projectId);
    
    emit projectDeleted(projectId);
    qCDebug(projectManager) << "Project deleted successfully:" << projectId;
    return true;
}

bool ProjectManager::updateProjectConfig(const QString &projectId, const QVariantMap &config)
{
    qCDebug(projectManager) << "Updating project config:" << projectId;
    
    if (!validateProjectConfig(config) || !hasProject(projectId)) {
        qCWarning(projectManager) << "Invalid parameters for updateProjectConfig";
        return false;
    }
    
    // Get project data from cache
    auto projectData = m_projectCache.value(projectId);
    if (!projectData) {
        qCWarning(projectManager) << "Project data not found in cache:" << projectId;
        return false;
    }
    
    // Update project configuration (only support database fields)
    if (config.contains("name")) {
        projectData->setName(config.value("name").toString());
    }
    
    // Update timestamp
    projectData->setUpdatedTime(QDateTime::currentDateTime());
    
    // Update in database
    if (!m_dbWrapper->updateProject(*projectData)) {
        qCWarning(projectManager) << "Failed to update project in database:" << projectId;
        return false;
    }
    
    emit projectUpdated(projectId);
    qCDebug(projectManager) << "Project config updated successfully:" << projectId;
    return true;
}

QVariantMap ProjectManager::getProjectInfo(const QString &projectId) const
{
    if (!hasProject(projectId))
        return QVariantMap();
    
    auto projectData = m_projectCache.value(projectId);
    if (projectData) {
        return projectData->toVariantMap();
    }
    
    return QVariantMap();
}

QVariantList ProjectManager::getAllProjects() const
{
    QVariantList projects;
    
    for (const QString &projectId : m_projectIds) {
        QVariantMap projectInfo = getProjectInfo(projectId);
        if (!projectInfo.isEmpty()) {
            projects.append(projectInfo);
        }
    }
    
    return projects;
}

bool ProjectManager::hasProject(const QString &projectId) const
{
    return m_projectIds.contains(projectId) && m_projectCache.contains(projectId);
}

QString ProjectManager::findProjectByModelId(const QString &modelId) const
{   
    // Search through cached projects for matching modelId
    for (const auto &projectData : m_projectCache) {
        if (projectData && projectData->getVectorModelId() == modelId) {
            return projectData->getId();
        }
    }
    
    return QString();
}

QString ProjectManager::createOrGetProject(const QString &modelId, const QVariantMap &config)
{
    qCDebug(projectManager) << "Create or get project with modelId:" << modelId << "for appId:" << m_appId;
    
    if (!m_isInitialized) {
        qCWarning(projectManager) << "ProjectManager not initialized for appId:" << m_appId;
        return QString();
    }

    if (modelId.isEmpty()) {
        qCWarning(projectManager) << "Invalid modelId for createOrGetProject:" << modelId;
        return QString();
    }

    // First, check if a project with this modelId already exists
    QString existingProjectId = findProjectByModelId(modelId);
    if (!existingProjectId.isEmpty()) {
        qCDebug(projectManager) << "Found existing project:" << existingProjectId << "for modelId:" << modelId;
        
        // Update last access time for existing project
        if (m_projectCache.contains(existingProjectId)) {
            auto projectData = m_projectCache[existingProjectId];
            if (projectData) {
                projectData->setLastAccessTime(QDateTime::currentDateTime());
                m_dbWrapper->updateProject(*projectData);
            }
        }
        
        return existingProjectId;
    }
    
    // No existing project found, create a new one
    qCDebug(projectManager) << "Creating new project for modelId:" << modelId;
    return createProject(modelId, config);
}

int ProjectManager::getProjectStatus(const QString &projectId) const
{
    if (!hasProject(projectId)) {
        return 0; // Invalid status
    }
    
    auto projectData = m_projectCache.value(projectId);
    if (projectData) {
        return static_cast<int>(projectData->getStatus());
    }
    
    return 0;
}

QVariantMap ProjectManager::getProjectStats(const QString &projectId) const
{
    if (!hasProject(projectId)) {
        return QVariantMap();
    }
    
    // TODO: Implement get project stats logic
    return QVariantMap();
}

int ProjectManager::getProjectCount() const
{
    return m_projectIds.size();
}

QStringList ProjectManager::getProjectIds() const
{
    return m_projectIds;
}

void ProjectManager::onProjectUpdated(const QString &projectId)
{
    qCDebug(projectManager) << "Project updated:" << projectId;
    updateProjectCache(projectId);
    emit projectUpdated(projectId);
}

void ProjectManager::refreshProjectCache()
{
    qCDebug(projectManager) << "Refreshing project cache for appId:" << m_appId;
    loadProjectsFromDatabase();
}

QString ProjectManager::generateProjectId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool ProjectManager::createProjectDirectory(const QString &projectId)
{
    QString projectDir = getProjectDirectory(projectId);
    QDir dir;
    return dir.mkpath(projectDir);
}

bool ProjectManager::removeProjectDirectory(const QString &projectId)
{
    QString projectDir = getProjectDirectory(projectId);
    QDir dir(projectDir);
    return dir.removeRecursively();
}

void ProjectManager::loadProjectsFromDatabase()
{
    qCDebug(projectManager) << "Loading projects from database for appId:" << m_appId;
    
    auto projects = m_dbWrapper->getProjectsByAppId(m_appId);
    
    m_projectIds.clear();
    m_projectCache.clear();
    
    for (const auto &project : projects) {
        if (project) {
            QString projectId = project->getId();
            m_projectIds.append(projectId);
            m_projectCache[projectId] = project;
        }
    }
    
    qCDebug(projectManager) << "Loaded" << m_projectIds.size() << "projects for appId:" << m_appId;
}

void ProjectManager::updateProjectCache(const QString &projectId)
{
    auto project = m_dbWrapper->getProject(projectId);
    if (project) {
        m_projectCache[projectId] = project;
    }
}

bool ProjectManager::validateProjectConfig(const QVariantMap &config) const
{
    // Simple validation for supported config parameters
    if (config.contains("projectName")) {
        QString projectName = config.value("projectName").toString();
        if (projectName.isEmpty() || projectName.length() > 255) {
            return false;
        }
    }
    return true;
}

QString ProjectManager::getProjectDirectory(const QString &projectId) const
{
    return m_projectPath + QDir::separator() + projectId;
}

QString ProjectManager::getRelativeProjectPath(const QString &projectId) const
{
    return QString("vectordb/%1/%2").arg(m_appId).arg(projectId);
}

QSharedPointer<ProjectData> ProjectManager::createProjectData(const QString &projectId, 
                                                              const QString &modelId, 
                                                              const QVariantMap &config)
{
    auto projectData = QSharedPointer<ProjectData>::create(this);
    
    // Set basic project information
    projectData->setId(projectId);
    projectData->setAppId(m_appId);
    projectData->setCreatedTime(QDateTime::currentDateTime());
    projectData->setUpdatedTime(QDateTime::currentDateTime());
    projectData->setLastAccessTime(QDateTime::currentDateTime());
    projectData->setStatus(ProjectData::ProjectStatus::Active);
    
    // Set model information
    projectData->setVectorModelId(modelId);
    
    // Set project configuration
    if (config.contains("projectName")) {
        projectData->setName(config.value("projectName").toString());
    } else {
        projectData->setName(QString("Project_%1").arg(projectId.left(8)));
    }

    projectData->setDir(getRelativeProjectPath(projectId));
    
    return projectData;
}
