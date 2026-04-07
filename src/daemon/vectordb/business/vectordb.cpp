// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vectordb.h"
#include "../data/datamanager.h"
#include "../index/vectorindexmanager.h"
#include "../service/serviceconfig.h"
#include "../task/workfloworchestrator.h"
#include "models/modelrepository.h"
#include "models/embeddingmodel.h"
#include "vectordb_define.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QFileInfo>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QJsonArray>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(vectorDb, "vectordb.business.vectordb")

VectorDb::VectorDb(const QString &workDir)
    : QObject(nullptr)
{
    qCDebug(vectorDb) << "VectorDb created for workDir:" << workDir;

    // Initialize work diretory
    m_workDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QDir::separator() + workDir;
    
    // Initialize components
    m_dataManager.reset(new DataManager(m_workDir));
    m_vectorIndexManager.reset(new VectorIndexManager(m_workDir));
    m_serviceConfig = ServiceConfig::instance();

    connectComponentSignals();
}

VectorDb::~VectorDb()
{
    qCDebug(vectorDb) << "VectorDb destroyed for workDir:" << m_workDir;
    cleanupCompletedWorkflows();
}

QString VectorDb::buildIndex(const QStringList &docId)
{
    qCDebug(vectorDb) << "Building index for workDir:" << m_workDir << "files count:" << docId.size();

    QStringList readyProcessDocIds;
    for (const QString &id : docId) {
        if (m_dataManager->isReadyForProcessing(id))
            readyProcessDocIds.append(id);
    }

    QStringList files = m_dataManager->getDocumentPath(readyProcessDocIds);

    if (files.isEmpty()) {
        qCWarning(vectorDb) << "No file for index build.";
        return QString();
    }

    // Create index building workflow using internal components
    QSharedPointer<WorkflowOrchestrator> workflow = createIndexBuildWorkflow(files);
    
    if (!workflow || workflow->getTotalStages() == 0) {
        qCWarning(vectorDb) << "Failed to create index building workflow for workDir:" << m_workDir;
        return QString();
    }
    
    // Register workflow for tracking
    registerWorkflow(workflow);
    
    // Start workflow execution
    if (!workflow->startWorkflow()) {
        qCWarning(vectorDb) << "Failed to start index building workflow for workDir:" << m_workDir;
        unregisterWorkflow(workflow->getWorkflowId());
        return QString();
    }
    
    qCDebug(vectorDb) << "Index building workflow started successfully, workflowId:" << workflow->getWorkflowId();
    return workflow->getWorkflowId();
}

QList<QVariantHash> VectorDb::uploadDocuments(const QStringList &files)
{
    qCDebug(vectorDb) << "Uploading files to workDir:" << m_workDir << "files count:" << files.size();

    if (files.isEmpty()) {
        qCWarning(vectorDb) << "No files provided for uploading documents";
        return {};
    }

    QList<QVariantHash> results;
    for (const QString &file : files) {
        // Add document to DataManager
        QString addedDocId = m_dataManager->addDocument(file);
        QVariantHash result;
        result["file"] = file;
        result["documentID"] = addedDocId;
        
        if (addedDocId.isEmpty()) {
            qCWarning(vectorDb) << "Failed to add document:" << file;
            result["status"] = -1; // Error code -1 for failure
            result["file_path"] = QString();
        } else {
            result["status"] = 0; // Error code 0 for success
            result["file_path"] = m_dataManager->getDocumentInfo(addedDocId).value("file_path").toString();;
        }
        results.append(result);
    }

    return results;
}

QList<QVariantHash> VectorDb::deleteDocuments(const QStringList &documentIds)
{
    qCDebug(vectorDb) << "Deleting documents from workDir:" << m_workDir << "documents count:" << documentIds.size();
    
    QList<QVariantHash> results;
    if (documentIds.isEmpty()) {
        qCWarning(vectorDb) << "No document IDs provided for deletion";
        return results;
    }
    
    if (!m_dataManager) {
        qCWarning(vectorDb) << "VectorIndexManager not available";
        return results;
    }

    if (!m_vectorIndexManager) {
        qCWarning(vectorDb) << "VectorIndexManager not available";
        return results;
    }
    
    try {
        for (const QString &docId : documentIds) {
            QVariantHash result;

            auto path = m_dataManager->getDocumentInfo(docId).value("file_path").toString();
            QString indexId = m_dataManager->getDocumentIndexId(docId);
            QVariantList chunks = m_dataManager->getDocumentChunks(docId);
            QStringList chunkIds;
            for (auto chunk : chunks) {
                chunkIds.append(chunk.toMap().value("id").toString());
            }

            result["indexId"] = indexId;
            result["documentID"] = docId;
            result["file_path"] = path;
            result["removeIndexStatus"] = m_vectorIndexManager->removeVectors(indexId, chunkIds);
            result["removeDocStatus"] = m_dataManager->removeDocument(docId) ? 0 : 1;
            results.append(result);
        }
        return results;
    } catch (const std::exception &e) {
        qCCritical(vectorDb) << "Exception in deleteDocuments:" << e.what();
        return results;
    }
}

bool VectorDb::deleteIndex()
{
    qCDebug(vectorDb) << "Deleting index for workDir:" << m_workDir;
    
    // Direct synchronous deletion using VectorIndexManager
    if (!m_vectorIndexManager) {
        qCWarning(vectorDb) << "VectorIndexManager not available";
        return false;
    }
    
    try {
        // Delete entire index using VectorIndexManager
        QString indexId = "default"; // Use default index for now
        bool success = m_vectorIndexManager->deleteIndex(indexId);
        if (!success) {
            qCWarning(vectorDb) << "Failed to delete index";
            return false;
        }
        
        qCDebug(vectorDb) << "Successfully deleted index for workDir:" << m_workDir;
        return true;
    } catch (const std::exception &e) {
        qCCritical(vectorDb) << "Exception in deleteIndex:" << e.what();
        return false;
    }
}

bool VectorDb::destroyIndex()
{
    qCDebug(vectorDb) << "Destroying index for workDir:" << m_workDir;
    
    if (!m_vectorIndexManager) {
        qCWarning(vectorDb) << "VectorIndexManager not available";
        return false;
    }
    if (!m_dataManager) {
        qCWarning(vectorDb) << "DataManager not available";
        return false;
    }

    try {
        disconnectComponentSignals();

        QDir dir(m_workDir);
        if (!dir.exists()) {
            qCWarning(vectorDb) << "WorkDir not exists:" << m_workDir;
            return false;
        }
        bool success = dir.removeRecursively();
        if (!success) {
            qCWarning(vectorDb) << "Failed to destroy index";
            return false;
        }
        return true;
    } catch (const std::exception &e) {
        qCCritical(vectorDb) << "Exception in destroyIndex:" << e.what();
        return false;
    }
}

QVariantList VectorDb::search(const QString &query, int topK, const QVariantMap &options)
{
    qCDebug(vectorDb) << "Searching in workDir:" << m_workDir << "query:" << query << "topK:" << topK;
    
    if (query.isEmpty()) {
        qCWarning(vectorDb) << "Empty query provided for search";
        return QVariantList();
    }
    
    if (!m_vectorIndexManager) {
        qCWarning(vectorDb) << "VectorIndexManager not available";
        return QVariantList();
    }
    
    try {
        ModelRepository respo;
        respo.refresh();
        EmbeddingModel *embedModel = respo.createModel(m_modelId, respo.getModelInfo(m_modelId));

        if (!embedModel) {
            qCWarning(vectorDb) << "Model create failed.";
            return QVariantList();
        }

        QJsonObject result = embedModel->embeddings(query);        
        delete embedModel;
        embedModel = nullptr;

        QJsonArray embeddingsArray = result.value("embeddings").toArray();
        QJsonArray vectors = embeddingsArray.first().toArray();
        QVector<float> vectorData;
        for (auto vector : vectors) {
            vectorData.push_back(vector.toDouble());
        }

        auto list = m_vectorIndexManager->search(vectorData);
        QVariantList ret;
        for (const QVariant &var : list) {
            QVariantMap info = var.toMap();
            QVariantMap chunk = m_dataManager->getChunkById(info.value("id").toString());
            chunk.insert("file", m_dataManager->getDocumentPath({chunk.value("document_id").toString()}).first());
            info.insert("chunk", chunk);
            info.insert(ExtensionParams::kModelKey, m_modelId);
            ret.append(info);
        }
        return ret;
    } catch (const std::exception &e) {
        qCCritical(vectorDb) << "Exception in search:" << e.what();
        return QVariantList();
    }
}

QString VectorDb::searchAsync(const QString &query, int topK, const QVariantMap &options)
{
    qCDebug(vectorDb) << "Async searching in workDir:" << m_workDir << "query:" << query << "topK:" << topK;
    
    if (query.isEmpty()) {
        qCWarning(vectorDb) << "Empty query provided for async search";
        return QString();
    }
    
    // Create search workflow using internal components
    QSharedPointer<WorkflowOrchestrator> workflow = createSearchWorkflow(query, topK, options);
    
    if (!workflow || workflow->getTotalStages() == 0) {
        qCWarning(vectorDb) << "Failed to create search workflow for workDir:" << m_workDir;
        return QString();
    }
    
    // Register workflow for tracking
    registerWorkflow(workflow);
    
    // Start workflow execution
    if (!workflow->startWorkflow()) {
        qCWarning(vectorDb) << "Failed to start search workflow for workDir:" << m_workDir;
        unregisterWorkflow(workflow->getWorkflowId());
        return QString();
    }
    
    qCDebug(vectorDb) << "Search workflow started successfully, workflowId:" << workflow->getWorkflowId();
    return workflow->getWorkflowId();
}

QVariantMap VectorDb::getIndexStatus() const
{
    qCDebug(vectorDb) << "Getting index status for workDir:" << m_workDir;
    
    // TODO: Implement get index status logic
    return QVariantMap();
}

QVariantMap VectorDb::getIndexStatistics() const
{
    qCDebug(vectorDb) << "Getting index statistics for workDir:" << m_workDir;
    
    // TODO: Implement get index statistics logic
    return QVariantMap();
}

QVariantMap VectorDb::getWorkflowProgress(const QString &workflowId) const
{
    QSharedPointer<WorkflowOrchestrator> workflow;
    
    {
        QMutexLocker locker(&const_cast<QMutex&>(m_workflowMutex));
        workflow = m_activeWorkflows.value(workflowId);
    }
    
    if (workflow) {
        return workflow->getWorkflowCheckpoint();
    }
    
    return QVariantMap();
}

QVariantMap VectorDb::getStageTaskProgress(const QString &taskId) const
{
    // Task progress is now handled within individual workflows
    // Find the workflow that contains this task
    QMutexLocker locker(&const_cast<QMutex&>(m_workflowMutex));
    for (auto it = m_activeWorkflows.begin(); it != m_activeWorkflows.end(); ++it) {
        if (it.value()) {
            // TODO: Add method to WorkflowOrchestrator to get task info by taskId
            // For now, return empty map
        }
    }
    
    return QVariantMap();
}

bool VectorDb::cancelWorkflow(const QString &workflowId)
{
    qCDebug(vectorDb) << "Cancelling workflow:" << workflowId;
    
    QSharedPointer<WorkflowOrchestrator> workflow;
    
    {
        QMutexLocker locker(&m_workflowMutex);
        workflow = m_activeWorkflows.value(workflowId);
    }
    
    if (workflow) {
        return workflow->cancelWorkflow();
    }
    
    return false;
}

bool VectorDb::pauseWorkflow(const QString &workflowId)
{
    qCDebug(vectorDb) << "Pausing workflow:" << workflowId;
    
    QSharedPointer<WorkflowOrchestrator> workflow;
    
    {
        QMutexLocker locker(&m_workflowMutex);
        workflow = m_activeWorkflows.value(workflowId);
    }
    
    if (workflow) {
        return workflow->pauseWorkflow();
    }
    
    return false;
}

bool VectorDb::resumeWorkflow(const QString &workflowId)
{
    qCDebug(vectorDb) << "Resuming workflow:" << workflowId;
    
    QSharedPointer<WorkflowOrchestrator> workflow;
    
    {
        QMutexLocker locker(&m_workflowMutex);
        workflow = m_activeWorkflows.value(workflowId);
    }
    
    if (workflow) {
        return workflow->resumeWorkflow();
    }
    
    return false;
}

QStringList VectorDb::getActiveWorkflows() const
{
    QMutexLocker locker(&const_cast<QMutex&>(m_workflowMutex));
    return m_activeWorkflows.keys();
}

bool VectorDb::isHealthy() const
{
    return m_serviceConfig && m_serviceConfig->isSystemHealthy();
}

QVariantMap VectorDb::getSystemStatus() const
{
    QVariantMap status;
    
    if (m_serviceConfig) {
        status = m_serviceConfig->getSystemStatus();
    }
    
    status["workDir"] = m_workDir;
    status["activeWorkflows"] = getActiveWorkflows().size();

    return status;
}

void VectorDb::setModelId(const QString &modelId)
{
    m_modelId = modelId;
}

QVariantList VectorDb::getAllDocuments() const
{
    qCDebug(vectorDb) << "Getting all documents for workDir:" << m_workDir;
    
    if (!m_dataManager) {
        qCWarning(vectorDb) << "DataManager not initialized";
        return QVariantList();
    }

    // insert model id.
    QVariantList ret;
    auto infos = m_dataManager->getAllDocuments();
    for (auto it = infos.begin(); it != infos.end(); ++it) {
        auto val = it->toHash();
        val.insert(ExtensionParams::kModelKey, m_modelId);
        ret.append(val);
    }

    return ret;
}

QVariantMap VectorDb::getDocumentInfo(const QString &documentId) const
{
    qCDebug(vectorDb) << "Getting document info for documentId:" << documentId << "workDir:" << m_workDir;
    
    if (!m_dataManager) {
        qCWarning(vectorDb) << "DataManager not initialized";
        return QVariantMap();
    }

    auto ret = m_dataManager->getDocumentInfo(documentId);
    if (!ret.isEmpty())
        ret.insert(ExtensionParams::kModelKey, m_modelId);
    return ret;
}

bool VectorDb::isRunning()
{
    bool isR = m_activeWorkflows.size() > 0;
    return isR;
}

void VectorDb::onWorkflowCompleted(const QString &workflowId, bool success)
{
    qCDebug(vectorDb) << "Workflow completed:" << workflowId << "success:" << success;
    
    QSharedPointer<WorkflowOrchestrator> workflow;
    
    {
        QMutexLocker locker(&m_workflowMutex);
        workflow = m_activeWorkflows.value(workflowId);
    }
    
    if (workflow) {
        // Handle workflow completion based on type
        WorkflowOrchestrator::WorkflowType workflowType = workflow->getWorkflowType();
        QString projectId = workflow->getWorkflowCheckpoint().value("projectId").toString();
        
        switch (workflowType) {
        case WorkflowOrchestrator::WorkflowType::IndexBuilding:
            if (processIndexBuildWorkflowRestults(workflow->getWorkflowResults())) {
                emit indexBuildWorkflowCompleted(projectId, workflowId, success,
                                               success ? "Index built successfully" : "Index build failed");
            }
            break;
        case WorkflowOrchestrator::WorkflowType::DocumentUpdate:
            // TODO: Handle document update completion
            break;
        case WorkflowOrchestrator::WorkflowType::Search:
            // TODO: Handle search completion
            break;
        case WorkflowOrchestrator::WorkflowType::Maintenance:
            // TODO: Handle maintenance completion
            break;
        }
        
        // 将 unregisterWorkflow 推迟到下一个事件循环，避免在信号处理函数中直接析构发送信号的对象
        QTimer::singleShot(0, this, [this, workflowId](){
            unregisterWorkflow(workflowId);
        });
    }
}

void VectorDb::onWorkflowProgress(const QString &workflowId, int completedStages, int totalStages)
{
    qCDebug(vectorDb) << "Workflow progress:" << workflowId << completedStages << "/" << totalStages;
    emit workflowProgress(workflowId, completedStages, totalStages);
}

void VectorDb::onWorkflowError(const QString &workflowId, const QString &error)
{
    qCWarning(vectorDb) << "Workflow error:" << workflowId << error;
    emit errorOccurred("workflow", error);
}

QSharedPointer<WorkflowOrchestrator> VectorDb::createIndexBuildWorkflow(const QStringList &files)
{
    qCDebug(vectorDb) << "Creating index build workflow for" << files.size() << "files";
    
    QVariantMap params;
    params.insert("data", files);
    params.insert("model", getModelId());
    // Create workflow using factory method
    QSharedPointer<WorkflowOrchestrator> workflow = 
        WorkflowOrchestrator::createWorkflow(WorkflowOrchestrator::WorkflowType::IndexBuilding, params);
    
    return workflow;
}

QSharedPointer<WorkflowOrchestrator> VectorDb::createDocumentUpdateWorkflow(const QStringList &files)
{
    qCDebug(vectorDb) << "Creating document update workflow for" << files.size() << "files";
    
    // Create workflow context with necessary components
    QVariantMap params;
    params.insert("data", files);
    params.insert("model", getModelId());
    
    // Create workflow using factory method
    QSharedPointer<WorkflowOrchestrator> workflow = 
        WorkflowOrchestrator::createWorkflow(WorkflowOrchestrator::WorkflowType::DocumentUpdate, params);
    
    return workflow;
}

QSharedPointer<WorkflowOrchestrator> VectorDb::createSearchWorkflow(const QString &query, int topK, const QVariantMap &options)
{
    qCDebug(vectorDb) << "Creating search workflow for query:" << query << "topK:" << topK;

    QVariantMap params;
    params.insert("data", query);
    params.insert("model", getModelId());
    params.insert("topK", topK);
    
    // Create workflow using factory method
    QSharedPointer<WorkflowOrchestrator> workflow = 
        WorkflowOrchestrator::createWorkflow(WorkflowOrchestrator::WorkflowType::Search, params);
    
    return workflow;
}

QSharedPointer<WorkflowOrchestrator> VectorDb::createDeleteWorkflow(const QStringList &documentIds)
{
    qCDebug(vectorDb) << "Creating delete workflow for" << documentIds.size() << "documents";
    
    QVariantMap params;
    params.insert("data", documentIds);
    
    // Create workflow using factory method
    QSharedPointer<WorkflowOrchestrator> workflow = 
        WorkflowOrchestrator::createWorkflow(WorkflowOrchestrator::WorkflowType::Delete, params);
    
    return workflow;
}

bool VectorDb::processIndexBuildWorkflowRestults(const QVariantMap &results)
{
    qCDebug(vectorDb) << "Processing index build workflow results";
    
    QVariantMap dataTask = results.value("Task_0").toMap();
    QVariantMap vectorTask = results.value("Task_1").toMap();

    if (!dataTask.value("success").toBool() || !vectorTask.value("success").toBool()) {
        qCWarning(vectorDb) << "One or more workflow tasks failed";
        return false;
    }

    // Process data parsing results (Task_0)
    bool dataSuccess = processDataParsingResults(dataTask);

    // Process vector indexing results (Task_1)
    QString indexId = processVectorIndexingResults(vectorTask);

    // Update doc process status
    QStringList docIds;
    QStringList processedFiles = dataTask.value("processedFiles").toStringList();
    for (const QString &filePath : processedFiles) {
        QString docId = m_dataManager->getDocumentIdByPath(QFileInfo(filePath).fileName());
        docIds.append(docId);

        // update indexId to dataDb
        m_dataManager->updateDocumentIndexId(docId, indexId);
    }
    bool statusUpdate = m_dataManager->updateProcessingStatus2Vector(docIds);

    bool overallSuccess = dataSuccess && !indexId.isEmpty() && statusUpdate;
    if (overallSuccess) {
        qCDebug(vectorDb) << "Index build workflow results processed successfully";
    } else {
        qCWarning(vectorDb) << "Index build workflow results processing completed with errors";
    }

    // TODO: process failed to callback.
    return overallSuccess;
}

bool VectorDb::processDataParsingResults(const QVariantMap &dataTask)
{
    QVariantList textChunks = dataTask.value("textChunks").toList();
    QStringList processedFiles = dataTask.value("processedFiles").toStringList();
    qCDebug(vectorDb) << "Processing data results:" << textChunks.size() << "chunks from" << processedFiles.size() << "files";

    bool success = true;

    // Store document content and chunks using DataManager
    for (const QString &filePath : processedFiles) {
        // Add document to DataManager
        QString docId = m_dataManager->getDocumentIdByPath(QFileInfo(filePath).fileName());

        // Filter chunks for this document
        QVariantList documentChunks;
        for (const QVariant &chunkVariant : textChunks) {
            QVariantMap chunk = chunkVariant.toMap();
            if (chunk.value("sourceFile").toString() == filePath) {
                // Prepare chunk data for storage
                QVariantMap chunkData;
                chunkData["id"] = chunk.value("chunkId").toString();
                chunkData["content"] = chunk.value("text").toString();
                chunkData["metadata"] = QVariantMap{
                    {"chunkIndex", chunk.value("chunkIndex")},
                    {"totalChunks", chunk.value("totalChunks")},
                    {"startPosition", chunk.value("startPosition")},
                    {"endPosition", chunk.value("endPosition")},
                    {"documentType", chunk.value("document_type")},
                    {"documentEncoding", chunk.value("document_encoding")}
                };
                documentChunks.append(chunkData);
            }
        }

        // Store document chunks
        if (!documentChunks.isEmpty()) {
            if (!m_dataManager->storeDocumentChunks(docId, documentChunks)) {
                qCWarning(vectorDb) << "Failed to store chunks for document:" << docId;
                success = false;
            }
        }
    }

    return success;
}

QString VectorDb::processVectorIndexingResults(const QVariantMap &vectorTask)
{
    QVariantList vectorResults = vectorTask.value("embeddings").toList();
    qCDebug(vectorDb) << "Processing vector results:" << vectorResults.size();

    QVariantMap indexConfig;
    indexConfig["type"] = static_cast<int>(VectorIndexManager::IndexType::Flat);
    indexConfig["dimension"] = vectorTask.value("dims").toInt();
    indexConfig["metric"] = static_cast<int>(VectorIndexManager::DistanceMetric::InnerProduct);

    // Create index
    QString indexId = m_vectorIndexManager->createIndex(indexConfig);
    if (indexId.isEmpty()) {
        qCWarning(vectorDb) << "Failed to create vector index:" << indexId;
        return QString();
    }

    // Add vectors to index
    if (!vectorResults.isEmpty()) {
        if (!m_vectorIndexManager->addVectors(indexId, vectorResults)) {
            qCWarning(vectorDb) << "Failed to add vectors to index:" << indexId;
            return QString();
        } else {
            qCDebug(vectorDb) << "Successfully added" << vectorResults.size() << "vectors to index:" << indexId;
        }
    }

    return indexId;
}

void VectorDb::registerWorkflow(QSharedPointer<WorkflowOrchestrator> workflow)
{
    if (!workflow) {
        return;
    }
    
    // Connect workflow signals outside of lock
    connect(workflow.data(), &WorkflowOrchestrator::workflowCompleted,
            this, &VectorDb::onWorkflowCompleted);
    connect(workflow.data(), &WorkflowOrchestrator::workflowProgress,
            this, &VectorDb::onWorkflowProgress);
    connect(workflow.data(), &WorkflowOrchestrator::workflowError,
            this, &VectorDb::onWorkflowError);
    
    {
        QMutexLocker locker(&m_workflowMutex);
        m_activeWorkflows[workflow->getWorkflowId()] = workflow;
    }
}

void VectorDb::unregisterWorkflow(const QString &workflowId)
{
    QMutexLocker locker(&m_workflowMutex);
    m_activeWorkflows.remove(workflowId);
}



void VectorDb::optimizeStageResourceAllocation()
{
    // TODO: Implement stage resource allocation optimization
}

void VectorDb::updateStageThreadCounts()
{
    // TODO: Implement stage thread count updates
}

void VectorDb::cleanupCompletedWorkflows()
{
    QMutexLocker locker(&m_workflowMutex);
    
    QStringList completedWorkflows;
    for (auto it = m_activeWorkflows.begin(); it != m_activeWorkflows.end(); ++it) {
        if (it.value() && it.value()->getStatus() == WorkflowOrchestrator::WorkflowStatus::Completed) {
            completedWorkflows.append(it.key());
        }
    }
    
    for (const QString &workflowId : completedWorkflows) {
        m_activeWorkflows.remove(workflowId);
    }
}

void VectorDb::connectComponentSignals()
{
    // Connect data manager signals
    if (m_dataManager) {
        connect(m_dataManager.data(), &DataManager::errorOccurred,
                this, &VectorDb::errorOccurred);
    }
    
    // Connect vector index manager signals
    if (m_vectorIndexManager) {
        connect(m_vectorIndexManager.data(), &VectorIndexManager::errorOccurred,
                this, &VectorDb::errorOccurred);
    }
}

void VectorDb::disconnectComponentSignals()
{
    // Disconnect data manager signals
    if (m_dataManager) {
        disconnect(m_dataManager.data(), nullptr, this, nullptr);
    }
    
    // Disconnect vector index manager signals
    if (m_vectorIndexManager) {
        disconnect(m_vectorIndexManager.data(), nullptr, this, nullptr);
    }
}
