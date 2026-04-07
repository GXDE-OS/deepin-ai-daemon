// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "session.h"
#include "project/projectmanager.h"
#include "service/serviceconfig.h"
#include "business/vectordb.h"
#include "vectordb_define.h"
#include "models/modelrepository.h"

#include <QLoggingCategory>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(session, "vectordb.session")

Session::Session(const QString &appId)
    : QObject(nullptr)
    , m_appId(appId)
    , m_lastAccessTime(QDateTime::currentDateTime())
{
    qCDebug(session) << "Session created for appId:" << appId;
}

Session::~Session()
{
    qCDebug(session) << "Session destroyed for appId:" << m_appId;
    cleanup();
}

bool Session::initialize()
{
    qCDebug(session) << "Initializing session for appId:" << m_appId;

    if (m_isInitialized) {
        qCDebug(session) << "Session already initialized for appId:" << m_appId;
        return true;
    }

    if (!initializeComponents()) {
        qCWarning(session) << "Failed to initialize components for appId:" << m_appId;
        return false;
    }

    connectSignals();

    m_isInitialized = true;
    updateLastAccessTime();

    qCDebug(session) << "Session initialized successfully for appId:" << m_appId;
    return true;
}

void Session::cleanup()
{
    qCDebug(session) << "Cleaning up session for appId:" << m_appId;

    // QSharedPointer will automatically clean up VectorDb objects
    m_vectorDbs.clear();

    // Components will be automatically cleaned up by QScopedPointer
    m_isInitialized = false;
}

bool Session::isActive() const
{
    for (auto it = m_vectorDbs.begin(); it != m_vectorDbs.end(); ++it) {
        if (it.value()->isRunning())
            return true;
    }

    return false;
}

QString Session::buildIndex(const QString &docId, const QVariantHash &extensionParams)
{
    qCDebug(session) << "Building index for appId:" << m_appId << "docId:" << docId;
    updateLastAccessTime();

    QString projectId = findProjectByDocument(docId).first;
    if (projectId.isEmpty()) {
        qCWarning(session) << "No projectId for doc:" << docId;
        return "";
    }

    // Get or create VectorDb for this project
    VectorDb* vectorDb = getOrCreateVectorDb(projectId);
    if (!vectorDb) {
        qCWarning(session) << "Failed to get or create VectorDb for projectId:" << projectId;
        return QString();
    }

    QString taskId = vectorDb->buildIndex(QStringList{docId});

    if (taskId.isEmpty()) {
        qCWarning(session) << "Failed to create build index task for projectId:" << projectId;
        return QString();
    }

    qCDebug(session) << "Successfully created build index task:" << taskId << "for projectId:" << projectId;
    return taskId;
}

bool Session::destroyIndex(bool allIndex, const QVariantHash &extensionParams)
{
    qCDebug(session) << "Deleting index for appId:" << m_appId;
    updateLastAccessTime();
    QStringList projectList;
    if (allIndex) {
       QVariantList allProjects = getAllProjects();
       for (const QVariant &var : allProjects) {
           QVariantHash info = var.toHash();
           auto id = info.value("id").toString();
           if (!id.isEmpty())
               projectList.append(id);
       }
    } else {
        QString modelId = ExtensionParams::getModel(extensionParams);
        QString projectId = findProjectByModelId(modelId);
        if (projectId.isEmpty()) {
            qCWarning(session) << "Failed to delete index for appId:" << m_appId
                               << "model" << modelId;
            return false;
        }
        projectList.append(projectId);
    }

    // TODO delete project, index, document.
    for (const QString &projectId : projectList) {
        VectorDb* vectorDb = getOrCreateVectorDb(projectId);
        if (!vectorDb) {
            qCWarning(session) << "VectorDb not found for projectId:" << projectId;
            continue;
        }

        vectorDb->destroyIndex();
        disconnect(vectorDb, nullptr, this, nullptr);

        removeVectorDb(projectId);
    }

    return true;
}

QVariantList Session::search(const QString &query, const QVariantHash &extensionParams)
{
    // Extract topK from extension parameters with default value
    int topK = ExtensionParams::getTopk(extensionParams, 10);

    qCDebug(session) << "Searching for appId:" << m_appId << "query:" << query << "topK:" << topK;
    updateLastAccessTime();
    QVariantList rets;

    if (query.isEmpty() || topK <= 0) {
        qCWarning(session) << "Invalid search parameters: query=" << query << "topK=" << topK;
        return rets;
    }

    QString modelId = ExtensionParams::getModel(extensionParams);
    QStringList projectList;

    if (modelId.isEmpty()) {
       // search all projects
       QVariantList allProjects = getAllProjects();
       for (const QVariant &var : allProjects) {
           QVariantHash info = var.toHash();
           auto id = info.value("id").toString();
           if (!id.isEmpty())
               projectList.append(id);
       }
    } else {
        QString projectId = findProjectByModelId(modelId);
        if (projectId.isEmpty()) {
            qCWarning(session) << "Failed to delete index for appId:" << m_appId
                               << "model" << modelId;
            return rets;
        }
        projectList.append(projectId);
    }

    for (const QString &projectId : projectList) {
        VectorDb* vectorDb = getOrCreateVectorDb(projectId);
        if (!vectorDb) {
            qCWarning(session) << "VectorDb not found for projectId:" << projectId;
            continue;
        }

        for (const QVariant &tmp : vectorDb->search(query, topK))
            rets.append(tmp);
    }

    return rets;
}

QString Session::searchAsync(const QString &query, const QVariantHash &extensionParams)
{
    // Extract topK from extension parameters with default value
    int topK = ExtensionParams::getTopk(extensionParams, 10);

    qCDebug(session) << "Async search for appId:" << m_appId << "query:" << query << "topK:" << topK;
    updateLastAccessTime();

    if (query.isEmpty()) {
        qCWarning(session) << "Invalid search parameters: query=" << query;
        return QString();
    }

    // Get or create project ID automatically
    QString projectId = findProjectByModelId(getValidModel(ExtensionParams::getModel(extensionParams)));
    if (projectId.isEmpty()) {
        qCWarning(session) << "Failed to get or create project for appId:" << m_appId;
        return QString();
    }

    VectorDb* vectorDb = getOrCreateVectorDb(projectId);
    if (!vectorDb) {
        qCWarning(session) << "Failed to get or create VectorDb for projectId:" << projectId;
        return QString();
    }

    try {
        // Perform async search
        // TODO: Implement async search through VectorDb
        // return vectorDb->searchAsync(query, topK);
        return QString();

    } catch (const std::exception &e) {
        qCCritical(session) << "Exception in searchAsync:" << e.what() << "for appId:" << m_appId;
        emit errorOccurred(QString("Async search failed: %1").arg(e.what()));
        return QString();
    } catch (...) {
        qCCritical(session) << "Unknown exception in searchAsync for appId:" << m_appId;
        emit errorOccurred("Async search failed: Unknown error");
        return QString();
    }
}

QString Session::createProject(const QString &modelId, const QVariantMap &config)
{
    qCDebug(session) << "Creating or getting project with modelId:" << modelId << "for appId:" << m_appId;
    updateLastAccessTime();

    if (!m_projectManager) {
        qCWarning(session) << "ProjectManager not initialized for appId:" << m_appId;
        return QString();
    }

    ModelRepository repo;
    repo.refresh();

    if (!repo.hasModel(modelId)) {
        qCWarning(session) << "Invalid modelId for createProject";
        return QString();
    }

    // Create or get existing project based on appId + modelId
    QString projectId = m_projectManager->createOrGetProject(modelId, config);
    if (projectId.isEmpty()) {
        qCWarning(session) << "Failed to create or get project with modelId:" << modelId;
        return QString();
    }

    // Create VectorDb object for this project if it doesn't exist
    if (!getOrCreateVectorDb(projectId)) {
        qCWarning(session) << "Failed to create VectorDb for projectId:" << projectId;
        // Clean up the project if VectorDb creation failed
        m_projectManager->deleteProject(projectId);
        return QString();
    }

    qCDebug(session) << "Project created/retrieved successfully:" << projectId << "for appId:" << m_appId << "modelId:" << modelId;
    return projectId;
}

QString Session::findProjectByModelId(const QString &modelId)
{
    qCDebug(session) << "Finding project by modelId:" << modelId << "for appId:" << m_appId;
    updateLastAccessTime();

    if (!m_projectManager) {
        qCWarning(session) << "ProjectManager not initialized for appId:" << m_appId;
        return QString();
    }

    if (modelId.isEmpty()) {
        qCWarning(session) << "Invalid modelId for findProjectByModelId";
        return QString();
    }

    QString projectId = m_projectManager->findProjectByModelId(modelId);
    if (!projectId.isEmpty()) {
        qCDebug(session) << "Found project:" << projectId << "for modelId:" << modelId;
    } else {
        qCDebug(session) << "No project found for modelId:" << modelId;
    }

    return projectId;
}

bool Session::deleteProject(const QString &projectId)
{
    qCDebug(session) << "Deleting project:" << projectId << "for appId:" << m_appId;
    updateLastAccessTime();

    if (!validateProjectId(projectId) || !m_projectManager) {
        qCWarning(session) << "Invalid projectId or ProjectManager not initialized";
        return false;
    }

    // Remove VectorDb object for this project first
    removeVectorDb(projectId);

    // Delete project through project manager
    bool success = m_projectManager->deleteProject(projectId);
    if (!success) {
        qCWarning(session) << "Failed to delete project:" << projectId;
        return false;
    }

    qCDebug(session) << "Project deleted successfully:" << projectId << "for appId:" << m_appId;
    return true;
}

QVariantMap Session::getProjectInfo(const QString &projectId)
{
    qCDebug(session) << "Getting project info:" << projectId << "for appId:" << m_appId;
    updateLastAccessTime();

    if (!validateProjectId(projectId) || !m_projectManager) {
        qCWarning(session) << "Invalid projectId or ProjectManager not initialized";
        return QVariantMap();
    }

    // Get project info through project manager
    QVariantMap projectInfo = m_projectManager->getProjectInfo(projectId);
    if (projectInfo.isEmpty()) {
        qCWarning(session) << "Project not found:" << projectId;
        return QVariantMap();
    }

    qCDebug(session) << "Retrieved project info for:" << projectId << "appId:" << m_appId;
    return projectInfo;
}

QVariantList Session::getAllProjects()
{
    qCDebug(session) << "Getting all projects for appId:" << m_appId;
    updateLastAccessTime();

    if (!m_projectManager) {
        qCWarning(session) << "ProjectManager not initialized for appId:" << m_appId;
        return QVariantList();
    }

    // Get all projects through project manager
    QVariantList projects = m_projectManager->getAllProjects();

    qCDebug(session) << "Retrieved" << projects.size() << "projects for appId:" << m_appId;
    return projects;
}

bool Session::updateProjectConfig(const QString &projectId, const QVariantMap &config)
{
    qCDebug(session) << "Updating project config:" << projectId << "for appId:" << m_appId;
    updateLastAccessTime();

    if (!validateProjectId(projectId) || !m_projectManager) {
        qCWarning(session) << "Invalid projectId or ProjectManager not initialized";
        return false;
    }

    // Update project config through project manager
    bool success = m_projectManager->updateProjectConfig(projectId, config);
    if (!success) {
        qCWarning(session) << "Failed to update project config:" << projectId;
        return false;
    }

    qCDebug(session) << "Project config updated successfully:" << projectId << "for appId:" << m_appId;
    return true;
}

QList<QVariantHash> Session::uploadDocuments(const QStringList &files, const QVariantHash &extensionParams)
{
    qCDebug(session) << "Uploading files for appId:" << m_appId << "count:" << files.size();
    updateLastAccessTime();

    if (files.isEmpty()) {
        qCWarning(session) << "Empty files list for uploadDocuments";
        return {};
    }

    // Get or create project ID automatically
    QString projectId = getOrCreateProjectId(getValidModel(ExtensionParams::getModel(extensionParams)));
    if (projectId.isEmpty()) {
        qCWarning(session) << "Failed to get or create project for appId:" << m_appId;
        return {};
    }

    VectorDb* vectorDb = getOrCreateVectorDb(projectId);
    if (!vectorDb) {
        qCWarning(session) << "Failed to get or create VectorDb for projectId:" << projectId;
        return {};
    }

    // Add doc and return results directly
    return vectorDb->uploadDocuments(files);
}

QList<QVariantHash> Session::deleteDocuments(const QStringList &documentIds)
{
    qCDebug(session) << "Deleting documents for appId:" << m_appId << "count:" << documentIds.size();
    updateLastAccessTime();

    if (documentIds.isEmpty()) {
        qCWarning(session) << "Empty documentIds list for deleteDocuments";
        return {};
    }

    QList<QVariantHash> results;
    for (const QString &docId : documentIds) {
        QVariantHash op;
        QString projectId = findProjectByDocument(docId).first;
        if (projectId.isEmpty()) {
            qCWarning(session) << "No project found for appId:" << m_appId << "docId:" << docId;
            op["documentID"] = docId;
            op["status"] = -1;
            results.append(op);
            continue;
        }

        VectorDb* vectorDb = getOrCreateVectorDb(projectId);
        if (!vectorDb) {
            qCWarning(session) << "VectorDb not found for projectId:" << projectId;
            op["documentID"] = docId;
            op["status"] = -2;
            results.append(op);
            continue;
        }

        auto ops = vectorDb->deleteDocuments({docId});
        if (!ops.isEmpty()) {
            results.append(ops.first());
        } else {
            op["documentID"] = docId;
            op["status"] = 1;
            results.append(op);
        }
    }

    return results;
}

QVariantMap Session::getIndexStatus()
{
    qCDebug(session) << "Getting index status for appId:" << m_appId;
    updateLastAccessTime();

    // Get current project ID (for now, we need to determine which project to get status for)
    // TODO: This method needs to be redesigned to accept projectId parameter
    QString projectId;
    if (projectId.isEmpty()) {
        qCWarning(session) << "No project found for appId:" << m_appId;
        return QVariantMap();
    }

    VectorDb* vectorDb = getOrCreateVectorDb(projectId);
    if (!vectorDb) {
        qCWarning(session) << "VectorDb not found for projectId:" << projectId;
        return QVariantMap();
    }

    // TODO: Implement get index status logic through VectorDb
    // return vectorDb->getIndexStatus();
    return QVariantMap();
}

QVariantMap Session::getIndexStatistics(const QString &projectId)
{
    qCDebug(session) << "Getting index statistics for projectId:" << projectId;
    updateLastAccessTime();

    if (!validateProjectId(projectId)) {
        return QVariantMap();
    }

    VectorDb* vectorDb = getOrCreateVectorDb(projectId);
    if (!vectorDb) {
        qCWarning(session) << "VectorDb not found for projectId:" << projectId;
        return QVariantMap();
    }

    // TODO: Implement get index statistics logic through VectorDb
    // return vectorDb->getIndexStatistics();
    return QVariantMap();
}

QVariantList Session::documentsInfo(const QStringList &documentIds)
{
    qCDebug(session) << "Getting documents info for appId:" << m_appId << "documentIds count:" << documentIds.size();
    updateLastAccessTime();

    if (!m_projectManager) {
        qCWarning(session) << "ProjectManager not initialized for appId:" << m_appId;
        return QVariantList();
    }

    // Get all projects for this session
    QVariantList projects = m_projectManager->getAllProjects();
    QVariantList allDocuments;

    // Iterate through all projects to get documents
    for (const QVariant &projectVariant : projects) {
        QVariantMap projectInfo = projectVariant.toMap();
        QString projectId = projectInfo.value("id").toString();
        
        if (projectId.isEmpty()) {
            continue;
        }

        // Get VectorDb instance for this project
        VectorDb* vectorDb = getOrCreateVectorDb(projectId);
        if (!vectorDb) {
            qCWarning(session) << "VectorDb not found for projectId:" << projectId;
            continue;
        }

        // Get documents from this project
        QVariantList projectDocuments;
        if (documentIds.isEmpty()) {
            // Get all documents if no specific IDs provided
            for (const QVariant &one : vectorDb->getAllDocuments())
                allDocuments.append(one.toMap());
        } else {
            // Get specific documents by IDs
            for (const QString &documentId : documentIds) {
                QVariantMap docInfo = vectorDb->getDocumentInfo(documentId);
                if (!docInfo.isEmpty()) {
                    allDocuments.append(docInfo);
                }
            }
        }
    }

    qCDebug(session) << "Retrieved" << allDocuments.size() << "documents for appId:" << m_appId;
    return allDocuments;
}

void Session::updateLastAccessTime()
{
    m_lastAccessTime = QDateTime::currentDateTime();
}

void Session::onTaskCompleted(const QString &taskId, bool success)
{
    qCDebug(session) << "Task completed:" << taskId << "success:" << success;
    updateLastAccessTime();

    // TODO: Handle task completion
}

bool Session::initializeComponents()
{
    // Initialize project manager
    m_projectManager.reset(new ProjectManager(m_appId));
    if (!m_projectManager->initialize()) {
        qCWarning(session) << "Failed to initialize ProjectManager for appId:" << m_appId;
        return false;
    }

    // Get shared service config (ServiceConfig is a singleton with private destructor)
    ServiceConfig *serviceConfigPtr = ServiceConfig::instance();
    if (!serviceConfigPtr) {
        qCWarning(session) << "Failed to get ServiceConfig instance for appId:" << m_appId;
        return false;
    }
    // Store as raw pointer since ServiceConfig manages its own lifecycle
    m_serviceConfig = serviceConfigPtr;

    // VectorDb objects will be created on-demand for each project
    // No need to initialize them here
    
    return true;
}

void Session::connectSignals()
{
    // Connect project manager signals
    if (m_projectManager) {
        connect(m_projectManager.data(), &ProjectManager::errorOccurred,
                this, &Session::errorOccurred);
    }

    // VectorDb signals will be connected when each VectorDb object is created
    // in getOrCreateVectorDb method

    qCDebug(session) << "Signals connected for appId:" << m_appId;
}

bool Session::validateProjectId(const QString &projectId) const
{
    return !projectId.isEmpty() && projectId.length() <= 64;
}

bool Session::validateTaskId(const QString &taskId) const
{
    return !taskId.isEmpty() && taskId.length() <= 64;
}

QString Session::getValidModel(const QString &model)
{
    if (model.isEmpty()) {
        //Get default model
        ModelRepository repo;
        repo.refresh();

        return repo.defaultModel();
    }
    return model;
}

QString Session::getOrCreateProjectId(const QString &modelId)
{
    if (!m_projectManager) {
        qCWarning(session) << "ProjectManager or EmbeddingModel not initialized for appId:" << m_appId;
        return QString();
    }

    // First try to find existing project by modelId
    QString projectId = findProjectByModelId(modelId);
    if (!projectId.isEmpty()) {
        qCDebug(session) << "Found existing project:" << projectId << "for appId:" << m_appId << "modelId:" << modelId;
        return projectId;
    }

    // If no project exists, create one with default configuration
    QVariantMap defaultConfig;
    defaultConfig["chunkSize"] = 512;
    defaultConfig["overlapSize"] = 50;
    defaultConfig["maxDocuments"] = 10000;

    projectId = createProject(modelId, defaultConfig);
    if (projectId.isEmpty()) {
        qCWarning(session) << "Failed to create project for appId:" << m_appId << "modelId:" << modelId;
        return QString();
    }

    qCDebug(session) << "Created new project:" << projectId << "for appId:" << m_appId << "modelId:" << modelId;
    return projectId;
}

VectorDb* Session::getOrCreateVectorDb(const QString &projectId)
{
    if (projectId.isEmpty()) {
        qCWarning(session) << "Invalid projectId for getOrCreateVectorDb";
        return nullptr;
    }

    // Check if VectorDb already exists for this project
    if (m_vectorDbs.contains(projectId)) {
        return m_vectorDbs[projectId].data();
    }

    // Get project directory from project manager
    QVariantMap projectInfo = m_projectManager->getProjectInfo(projectId);
    QString projectDir = projectInfo.value("dataDirectory").toString();
    QString modelId = projectInfo.value("vectorModelId").toString();
    if (projectDir.isEmpty()) {
        qCWarning(session) << "Failed to get project directory for projectId:" << projectId;
        return nullptr;
    }

    QSharedPointer<VectorDb> vectorDb;
    vectorDb.reset(new VectorDb(projectDir));
    vectorDb->setModelId(modelId);
    m_vectorDbs.insert(projectId, vectorDb);

    // Connect signals for this VectorDb
    connect(vectorDb.data(), &VectorDb::indexBuildWorkflowCompleted, this, &Session::indexBuildCompleted);
    connect(vectorDb.data(), &VectorDb::searchCompleted, this, &Session::searchCompleted);
    connect(vectorDb.data(), &VectorDb::errorOccurred, this, &Session::errorOccurred);

    qCDebug(session) << "Created VectorDb for projectId:" << projectId << "appId:" << m_appId;
    return vectorDb.data();
}

bool Session::removeVectorDb(const QString &projectId)
{
    if (projectId.isEmpty()) {
        qCWarning(session) << "Invalid projectId for removeVectorDb";
        return false;
    }

    if (!m_vectorDbs.contains(projectId)) {
        qCDebug(session) << "VectorDb not found for projectId:" << projectId;
        return true; // Not an error if it doesn't exist
    }

    // Remove and automatically delete the VectorDb object (QSharedPointer handles cleanup)
    m_vectorDbs.remove(projectId);

    qCDebug(session) << "Removed VectorDb for projectId:" << projectId << "appId:" << m_appId;
    return true;
}

QPair<QString, QVariantMap> Session::findProjectByDocument(const QString &docId)
{
    QPair<QString, QVariantMap> ret;
    qCDebug(session) << "find project id by document" << docId;
    updateLastAccessTime();

    if (docId.isEmpty() || !m_projectManager) {
        qCWarning(session) << "ProjectManager not initialized for appId:" << m_appId;
        return ret;
    }

    // Get all projects for this session
    QVariantList projects = m_projectManager->getAllProjects();
    QVariantList allDocuments;

    // Iterate through all projects to get documents
    for (const QVariant &projectVariant : projects) {
        QVariantMap projectInfo = projectVariant.toMap();
        QString projectId = projectInfo.value("id").toString();

        if (projectId.isEmpty()) {
            continue;
        }

        // Get VectorDb instance for this project
        VectorDb* vectorDb = getOrCreateVectorDb(projectId);
        if (!vectorDb) {
            qCWarning(session) << "VectorDb not found for projectId:" << projectId;
            continue;
        }

        QVariantMap docInfo = vectorDb->getDocumentInfo(docId);
        if (!docInfo.isEmpty()) {
            return qMakePair(projectId, docInfo);
        }
    }

    return ret;
}
