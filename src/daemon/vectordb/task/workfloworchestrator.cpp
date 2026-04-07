// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "workfloworchestrator.h"

#include <QLoggingCategory>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(workflowOrchestrator, "vectordb.task.workflow")

WorkflowOrchestrator::WorkflowOrchestrator(WorkflowType type)
    : QObject(nullptr)
    , m_workflowId(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_workflowType(type)
{
    qCDebug(workflowOrchestrator) << "WorkflowOrchestrator created:" << m_workflowId << "type:" << static_cast<int>(type);
    
    // Initialize task manager
    m_taskManager.reset(new StageBasedTaskManager());
    
    // Connect task manager signals
    connect(m_taskManager.data(), &StageBasedTaskManager::stageTaskCompleted,
            this, &WorkflowOrchestrator::onStageTaskCompleted);
    
    initializeCheckpointFile();
}

WorkflowOrchestrator::~WorkflowOrchestrator()
{
    qCDebug(workflowOrchestrator) << "WorkflowOrchestrator destroyed:" << m_workflowId;
}

bool WorkflowOrchestrator::startWorkflow()
{
    qCDebug(workflowOrchestrator) << "Starting workflow:" << m_workflowId;
    
    if (m_status != WorkflowStatus::Pending) {
        qCWarning(workflowOrchestrator) << "Workflow not in pending state:" << m_workflowId;
        return false;
    }
    
    m_startTime = QDateTime::currentDateTime();
    setStatus(WorkflowStatus::Running);
    
    emit workflowStarted(m_workflowId);
    
    // Start processing stages
    processNextStage();
    
    return true;
}

bool WorkflowOrchestrator::pauseWorkflow()
{
    qCDebug(workflowOrchestrator) << "Pausing workflow:" << m_workflowId;
    
    if (m_status != WorkflowStatus::Running) {
        return false;
    }
    
    setStatus(WorkflowStatus::Paused);
    
    // TODO: Pause current running tasks
    
    return true;
}

bool WorkflowOrchestrator::resumeWorkflow()
{
    qCDebug(workflowOrchestrator) << "Resuming workflow:" << m_workflowId;
    
    if (m_status != WorkflowStatus::Paused) {
        return false;
    }
    
    setStatus(WorkflowStatus::Running);
    
    // TODO: Resume paused tasks
    processNextStage();
    
    return true;
}

bool WorkflowOrchestrator::cancelWorkflow()
{
    qCDebug(workflowOrchestrator) << "Cancelling workflow:" << m_workflowId;
    
    if (m_status == WorkflowStatus::Completed || m_status == WorkflowStatus::Cancelled) {
        return false;
    }
    
    setStatus(WorkflowStatus::Cancelled);
    
    // TODO: Cancel current running tasks
    
    emit workflowCompleted(m_workflowId, false);
    
    return true;
}

qreal WorkflowOrchestrator::getOverallProgress() const
{
    if (m_totalStages == 0) {
        return 0.0;
    }
    
    return (static_cast<qreal>(m_completedStages) / m_totalStages) * 100.0;
}

void WorkflowOrchestrator::addStageStep(StageBasedTaskManager::ProcessingStage stage, const QVariantMap &taskParams, 
                                        StageBasedTaskManager::StagePriority priority)
{
    WorkflowStage workflowStage;
    workflowStage.stage = stage;
    workflowStage.taskParams = taskParams;
    workflowStage.priority = priority;
    
    m_stages.append(workflowStage);
    m_totalStages = m_stages.size();
}

void WorkflowOrchestrator::addDependency(int stageIndex, int dependsOnStageIndex)
{
    if (stageIndex >= 0 && stageIndex < m_stages.size() && 
        dependsOnStageIndex >= 0 && dependsOnStageIndex < m_stages.size()) {
        m_stages[stageIndex].dependencies.insert(dependsOnStageIndex);
    }
}

bool WorkflowOrchestrator::isStageReady(int stageIndex) const
{
    if (stageIndex < 0 || stageIndex >= m_stages.size()) {
        return false;
    }
    
    const WorkflowStage &stage = m_stages[stageIndex];
    
    // Check if all dependencies are completed
    for (int depIndex : stage.dependencies) {
        if (depIndex >= 0 && depIndex < m_stages.size()) {
            if (!m_stages[depIndex].isCompleted) {
                return false;
            }
        }
    }
    
    return true;
}

bool WorkflowOrchestrator::saveWorkflowState() const
{
    // TODO: Implement workflow state saving
    updateCheckpointFile();
    return true;
}

bool WorkflowOrchestrator::loadWorkflowState()
{
    // TODO: Implement workflow state loading
    return true;
}

QVariantMap WorkflowOrchestrator::getWorkflowCheckpoint() const
{
    QVariantMap checkpoint;
    
    checkpoint["workflowId"] = m_workflowId;
    checkpoint["workflowType"] = static_cast<int>(m_workflowType);
    checkpoint["status"] = static_cast<int>(m_status);
    checkpoint["completedStages"] = m_completedStages;
    checkpoint["totalStages"] = m_totalStages;
    checkpoint["currentStageIndex"] = m_currentStageIndex;
    checkpoint["startTime"] = m_startTime;
    checkpoint["endTime"] = m_endTime;
    checkpoint["overallProgress"] = getOverallProgress();
    
    // Add stage information
    QVariantList stageList;
    for (int i = 0; i < m_stages.size(); ++i) {
        const WorkflowStage &stage = m_stages[i];
        QVariantMap stageInfo;
        stageInfo["index"] = i;
        stageInfo["stage"] = static_cast<int>(stage.stage);
        stageInfo["taskId"] = stage.taskId;
        stageInfo["isCompleted"] = stage.isCompleted;
        stageInfo["hasFailed"] = stage.hasFailed;
        stageList.append(stageInfo);
    }
    checkpoint["stages"] = stageList;
    
    return checkpoint;
}

void WorkflowOrchestrator::onStageTaskCompleted(const QString &taskId, StageBasedTaskManager::ProcessingStage stage, bool success, const QVariantMap &results)
{
    qCDebug(workflowOrchestrator) << "Stage task completed:" << taskId << "stage:" << static_cast<int>(stage) 
                                  << "success:" << success << "results keys:" << results.keys() << "in workflow:" << m_workflowId;
    
    // Find the stage that completed
    for (int i = 0; i < m_stages.size(); ++i) {
        if (m_stages[i].taskId == taskId) {
            if (success) {
                // Validate results based on stage type
                QVariantMap validatedResults = validateStageResults(stage, results);
                if (!validatedResults.isEmpty() || isResultsOptional(stage)) {
                    completeStage(i, validatedResults);
                } else {
                    failStage(i, QString("Invalid results from stage %1").arg(static_cast<int>(stage)));
                }
            } else {
                failStage(i, "Stage task failed");
            }
            break;
        }
    }
}

void WorkflowOrchestrator::onStageTaskProgress(const QString &taskId, int percentage)
{
    // TODO: Handle individual stage progress updates
    Q_UNUSED(taskId)
    Q_UNUSED(percentage)
}

void WorkflowOrchestrator::processNextStage()
{
    if (m_status != WorkflowStatus::Running) {
        return;
    }
    
    // Find next ready stage
    for (int i = 0; i < m_stages.size(); ++i) {
        if (!m_stages[i].isCompleted && !m_stages[i].hasFailed && isStageReady(i)) {
            startStage(i);
            return;
        }
    }
    
    // Check if all stages are completed
    bool allCompleted = true;
    bool anyFailed = false;
    
    for (const WorkflowStage &stage : m_stages) {
        if (!stage.isCompleted) {
            allCompleted = false;
        }
        if (stage.hasFailed) {
            anyFailed = true;
        }
    }
    
    if (allCompleted || anyFailed) {
        m_endTime = QDateTime::currentDateTime();
        
        if (anyFailed) {
            setStatus(WorkflowStatus::Failed);
            emit workflowCompleted(m_workflowId, false);
        } else {
            setStatus(WorkflowStatus::Completed);
            m_workflowResults = collectWorkflowResults();
            emit workflowCompleted(m_workflowId, true);
        }
    }
}

bool WorkflowOrchestrator::canStartStage(int stageIndex) const
{
    return isStageReady(stageIndex) && !m_stages[stageIndex].isCompleted && !m_stages[stageIndex].hasFailed;
}

void WorkflowOrchestrator::startStage(int stageIndex)
{
    if (!canStartStage(stageIndex)) {
        return;
    }
    
    if (!m_taskManager) {
        qCWarning(workflowOrchestrator) << "No task manager available for workflow:" << m_workflowId;
        failStage(stageIndex, "Task manager not available");
        return;
    }
    
    WorkflowStage &stage = m_stages[stageIndex];
    m_currentStageIndex = stageIndex;
    
    // Create actual stage task based on stage type
    QString taskId;
    
    switch (stage.stage) {
    case StageBasedTaskManager::ProcessingStage::DataParsing: {
        QStringList files = stage.taskParams.value("files").toStringList();
        taskId = m_taskManager->createDataParserTask(stage.taskParams, stage.priority);
        break;
    }
    case StageBasedTaskManager::ProcessingStage::EmbeddingGeneration: {
        QVariantList textChunks = stage.taskParams.value("textChunks").toList();
        QString modelId = stage.taskParams.value("modelId").toString();
        // If textChunks is empty, try to get from previous stage results
        if (textChunks.isEmpty() && stageIndex > 0) {
            QString sourceStage = stage.taskParams.value("source_stage").toString();
            if (!sourceStage.isEmpty()) {
                QVariantMap previousResults = getStageResultsFromPrevious(stageIndex, sourceStage);
                if (!previousResults.isEmpty()) {
                    textChunks = previousResults.value("textChunks").toList();
                    qCDebug(workflowOrchestrator) << "Retrieved" << textChunks.size() << "text chunks from" << sourceStage << "stage";
                }
            }
        }
        QVariantMap params;
        params.insert("textChunks", textChunks);
        params.insert("modelId", modelId);
        taskId = m_taskManager->createEmbeddingTask(params, stage.priority);
        break;
    }
    case StageBasedTaskManager::ProcessingStage::IndexBuilding: {
        QVariantList vectors = stage.taskParams.value("vectors").toList();
        // If vectors is empty, try to get from previous stage results
        if (vectors.isEmpty() && stageIndex > 0) {
            QString sourceStage = stage.taskParams.value("source_stage").toString();
            if (!sourceStage.isEmpty()) {
                QVariantMap previousResults = getStageResultsFromPrevious(stageIndex, sourceStage);
                if (!previousResults.isEmpty()) {
                    vectors = previousResults.value("embeddings").toList();
                    qCDebug(workflowOrchestrator) << "Retrieved" << vectors.size() << "vectors from" << sourceStage << "stage";
                }
            }
        }
        taskId = m_taskManager->createIndexBuildTask(stage.taskParams, stage.priority);
        break;
    }
    case StageBasedTaskManager::ProcessingStage::Search: {
        QString query = stage.taskParams.value("query").toString();
        taskId = m_taskManager->createSearchTask(stage.taskParams, stage.priority);
        break;
    }
    case StageBasedTaskManager::ProcessingStage::DataPersistence: {
        QVariantMap persistenceData = stage.taskParams;
        // If no specific data, collect from previous stages
        if (persistenceData.value("data").toMap().isEmpty() && stageIndex > 0) {
            QString sourceStage = stage.taskParams.value("source_stage").toString();
            if (!sourceStage.isEmpty()) {
                QVariantMap previousResults = getStageResultsFromPrevious(stageIndex, sourceStage);
                if (!previousResults.isEmpty()) {
                    persistenceData["data"] = previousResults;
                    qCDebug(workflowOrchestrator) << "Retrieved persistence data from" << sourceStage << "stage:" << previousResults.keys();
                }
            }
        }
        taskId = m_taskManager->createPersistenceTask(persistenceData, stage.priority);
        break;
    }
    default:
        qCWarning(workflowOrchestrator) << "Unknown stage type:" << static_cast<int>(stage.stage);
        failStage(stageIndex, "Unknown stage type");
        return;
    }
    
    if (taskId.isEmpty()) {
        qCWarning(workflowOrchestrator) << "Failed to create task for stage" << stageIndex << "in workflow:" << m_workflowId;
        failStage(stageIndex, "Failed to create stage task");
        return;
    }
    
    stage.taskId = taskId;
    
    emit workflowStageStarted(m_workflowId, stageIndex, stage.stage);
    
    qCDebug(workflowOrchestrator) << "Started stage" << stageIndex << "with task" << taskId << "in workflow:" << m_workflowId;
}

void WorkflowOrchestrator::completeStage(int stageIndex, const QVariantMap &results)
{
    if (stageIndex < 0 || stageIndex >= m_stages.size()) {
        return;
    }
    
    WorkflowStage &stage = m_stages[stageIndex];
    stage.isCompleted = true;
    stage.results = results;
    
    m_completedStages++;
    
    emit workflowStageCompleted(m_workflowId, stageIndex, true);
    updateWorkflowProgress();
    
    // Process next stage
    processNextStage();
}

void WorkflowOrchestrator::failStage(int stageIndex, const QString &error)
{
    if (stageIndex < 0 || stageIndex >= m_stages.size()) {
        return;
    }
    
    WorkflowStage &stage = m_stages[stageIndex];
    stage.hasFailed = true;
    
    emit workflowStageCompleted(m_workflowId, stageIndex, false);
    emit workflowError(m_workflowId, error);
    
    // Fail the entire workflow
    setStatus(WorkflowStatus::Failed);
    emit workflowCompleted(m_workflowId, false);
}

void WorkflowOrchestrator::updateWorkflowProgress()
{
    emit workflowProgress(m_workflowId, m_completedStages, m_totalStages);
    saveWorkflowState();
}

QVariantMap WorkflowOrchestrator::collectWorkflowResults() const
{
    QVariantMap results;
    
    for (int i = 0; i < m_stages.size(); ++i) {
        const WorkflowStage &stage = m_stages[i];
        if (stage.isCompleted && !stage.results.isEmpty()) {
            results[QString("Task_%1").arg(i)] = stage.results;
        }
    }
    
    return results;
}

void WorkflowOrchestrator::initializeCheckpointFile()
{
    QString checkpointDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/vectordb/workflows";
    QDir dir;
    if (!dir.exists(checkpointDir)) {
        dir.mkpath(checkpointDir);
    }
    
    m_checkpointFilePath = QString("%1/%2.workflow").arg(checkpointDir, m_workflowId);
}

void WorkflowOrchestrator::updateCheckpointFile() const
{
    // TODO: Implement checkpoint file update
}

void WorkflowOrchestrator::setStatus(WorkflowStatus status)
{
    if (m_status != status) {
        m_status = status;
        qCDebug(workflowOrchestrator) << "Workflow status changed:" << m_workflowId << "status:" << static_cast<int>(status);
    }
}

void WorkflowOrchestrator::setupIndexBuildingWorkflow(const QVariantMap &params)
{
    qCDebug(workflowOrchestrator) << "Setting up index building workflow (simplified - only time-consuming tasks)";

    // Clear any existing stages
    m_stages.clear();
    m_totalStages = 0;
    m_completedStages = 0;

    QStringList files = params["data"].toStringList();
    
    // Task 1: Data Parsing - Extract text from documents
    QVariantMap parsingParams;
    parsingParams["files"] = files;
    parsingParams["outputFormat"] = "text_chunks";

    addStageStep(StageBasedTaskManager::ProcessingStage::DataParsing, 
                 parsingParams, StageBasedTaskManager::StagePriority::Low);
    
    // Task 2: Embedding Generation - Convert text chunks to vectors
    QVariantMap embeddingParams;
    embeddingParams["source_stage"] = "data_parsing";
    embeddingParams["modelId"] = params.value("model").toString();
    
    // Apply defaults if not configured
    if (!embeddingParams.contains("embedding_model")) {
        embeddingParams["embedding_model"] = "default";
    }
    if (!embeddingParams.contains("batch_size")) {
        embeddingParams["batch_size"] = 100;
    }
    
    addStageStep(StageBasedTaskManager::ProcessingStage::EmbeddingGeneration, 
                 embeddingParams, StageBasedTaskManager::StagePriority::Normal);
    
    // Set up task dependencies (sequential execution)
    addDependency(1, 0);  // Embedding depends on Data Parsing
    
    qCDebug(workflowOrchestrator) << "Setup index building workflow with" << m_totalStages;
}

QSharedPointer<WorkflowOrchestrator> WorkflowOrchestrator::createWorkflow(WorkflowType workflowType, const QVariantMap &params)
{
    // Create workflow instance with specified type
    QSharedPointer<WorkflowOrchestrator> workflow(
        new WorkflowOrchestrator(workflowType)
    );
    
    switch (workflowType) {
    case WorkflowType::IndexBuilding:
        // For index building, data contains documents
        workflow->setupIndexBuildingWorkflow(params);
        break;
    case WorkflowType::DocumentUpdate:
        // For document update, data contains updated documents
        workflow->setupDocumentUpdateWorkflow(params);
        break;
    case WorkflowType::Search:
        // For search, data contains queries
        workflow->setupSearchWorkflow(params);
        break;
    case WorkflowType::Delete:
        // For delete, data contains document IDs or empty for full delete
        workflow->setupDeleteWorkflow(params);
        break;
    case WorkflowType::Maintenance:
        // For maintenance, data is optional
        workflow->setupMaintenanceWorkflow(params);
        break;
    default:
        qCWarning(workflowOrchestrator) << "Unknown workflow type:" << static_cast<int>(workflowType);
        return nullptr;
    }
    
    return workflow;
}

void WorkflowOrchestrator::setupDocumentUpdateWorkflow(const QVariantMap &params)
{
    qCDebug(workflowOrchestrator) << "Setting up document update workflow";

    // Clear any existing stages
    m_stages.clear();
    m_totalStages = 0;
    m_completedStages = 0;

    QVariantMap options = params["options"].toMap();
    QStringList files = params["data"].toStringList();
    
    // Stage 1: Data Parsing - Extract text from new documents
    QVariantMap parsingParams;
    parsingParams["files"] = files;
    parsingParams["outputFormat"] = "text_chunks";
    
    addStageStep(StageBasedTaskManager::ProcessingStage::DataParsing, 
                 parsingParams, StageBasedTaskManager::StagePriority::Normal);
    
    // Stage 2: Embedding Generation - Convert new text chunks to vectors
    QVariantMap embeddingParams;
    embeddingParams["source_stage"] = "data_parsing";
    embeddingParams["embedding_model"] = options.value("embedding_model", "default");
    embeddingParams["batch_size"] = options.value("batch_size", 100);
    
    addStageStep(StageBasedTaskManager::ProcessingStage::EmbeddingGeneration, 
                 embeddingParams, StageBasedTaskManager::StagePriority::Normal);
    
    // Stage 3: Index Update - Add new vectors to existing index
    QVariantMap indexParams;
    indexParams["source_stage"] = "embedding_generation";
    indexParams["operation"] = "update";
    
    addStageStep(StageBasedTaskManager::ProcessingStage::IndexBuilding, 
                 indexParams, StageBasedTaskManager::StagePriority::Normal);
    
    // Stage 4: Data Persistence - Save updated index
    QVariantMap persistenceParams;
    persistenceParams["source_stage"] = "index_building";
    persistenceParams["persist_index"] = true;
    persistenceParams["persist_metadata"] = true;
    
    addStageStep(StageBasedTaskManager::ProcessingStage::DataPersistence,
                 persistenceParams, StageBasedTaskManager::StagePriority::Low);
    
    // Set up stage dependencies
    addDependency(1, 0);  // Embedding depends on Data Parsing
    addDependency(2, 1);  // Index update depends on Embedding
    addDependency(3, 2);  // Persistence depends on Index update
    
    qCDebug(workflowOrchestrator) << "Setup document update workflow with" << m_totalStages;
}

void WorkflowOrchestrator::setupSearchWorkflow(const QVariantMap &params)
{
    qCDebug(workflowOrchestrator) << "Setting up search workflow (simplified - only embedding task)";

    // Clear any existing stages
    m_stages.clear();
    m_totalStages = 0;
    m_completedStages = 0;

    QStringList queries = params["data"].toStringList();
    
    // Task 1: Query Embedding - Convert search query to vector
    QVariantMap embeddingParams;
    embeddingParams["queries"] = queries;
    embeddingParams["embedding_model"] = "default";
    
    addStageStep(StageBasedTaskManager::ProcessingStage::EmbeddingGeneration, 
                 embeddingParams, StageBasedTaskManager::StagePriority::High);
    
    // Note: 向量搜索不作为单独任务，在embedding完成后直接使用VectorIndexManager搜索
    
    qCDebug(workflowOrchestrator) << "Setup search workflow with" << m_totalStages;
}

void WorkflowOrchestrator::setupDeleteWorkflow(const QVariantMap &params)
{
    qCDebug(workflowOrchestrator) << "Setting up delete workflow (no tasks - direct execution)";

    // Clear any existing stages
    m_stages.clear();
    m_totalStages = 0;
    m_completedStages = 0;

    // TODO: Get components from workflow context
    
    qCDebug(workflowOrchestrator) << "Setup delete workflow with" << m_totalStages;
}

void WorkflowOrchestrator::setupMaintenanceWorkflow(const QVariantMap &params)
{
    Q_UNUSED(params)
    
    // TODO: Setup maintenance workflow stages
}

QVariantMap WorkflowOrchestrator::validateStageResults(StageBasedTaskManager::ProcessingStage stage, const QVariantMap &results) const
{
    QVariantMap validatedResults = results;
    
    switch (stage) {
    case StageBasedTaskManager::ProcessingStage::DataParsing:
        // Validate data parsing results
        if (results.contains("textChunks") && results.contains("processedFiles")) {
            QVariantList chunks = results.value("textChunks").toList();
            QStringList files = results.value("processedFiles").toStringList();
            
            if (!chunks.isEmpty() && !files.isEmpty()) {
                qCDebug(workflowOrchestrator) << "Data parsing results validated:" << chunks.size() << "chunks from" << files.size() << "files";
                return validatedResults;
            }
        }
        qCWarning(workflowOrchestrator) << "Invalid data parsing results - missing textChunks or processedFiles";
        return QVariantMap();
        
    case StageBasedTaskManager::ProcessingStage::EmbeddingGeneration:
        // Validate embedding results
        if (results.contains("embeddings") && results.contains("embeddingCount")) {
            QVariantList embeddings = results.value("embeddings").toList();
            int count = results.value("embeddingCount").toInt();
            
            if (!embeddings.isEmpty() && count > 0) {
                qCDebug(workflowOrchestrator) << "Embedding results validated:" << count << "embeddings generated";
                return validatedResults;
            }
        }
        qCWarning(workflowOrchestrator) << "Invalid embedding results - missing embeddings or embeddingCount";
        return QVariantMap();
        
    case StageBasedTaskManager::ProcessingStage::IndexBuilding:
        // Validate index building results
        if (results.contains("indexPath") && results.contains("indexedVectors")) {
            QString indexPath = results.value("indexPath").toString();
            int vectorCount = results.value("indexedVectors").toInt();
            
            if (!indexPath.isEmpty() && vectorCount > 0) {
                qCDebug(workflowOrchestrator) << "Index building results validated:" << vectorCount << "vectors indexed at" << indexPath;
                return validatedResults;
            }
        }
        qCWarning(workflowOrchestrator) << "Invalid index building results - missing indexPath or indexedVectors";
        return QVariantMap();
        
    case StageBasedTaskManager::ProcessingStage::Search:
        // Validate search results
        if (results.contains("searchResults") && results.contains("resultCount")) {
            QVariantList searchResults = results.value("searchResults").toList();
            int count = results.value("resultCount").toInt();
            
            qCDebug(workflowOrchestrator) << "Search results validated:" << count << "results found";
            return validatedResults;
        }
        qCWarning(workflowOrchestrator) << "Invalid search results - missing searchResults or resultCount";
        return QVariantMap();
        
    case StageBasedTaskManager::ProcessingStage::DataPersistence:
        // Validate persistence results
        if (results.contains("persistedData") && results.contains("persistenceStatus")) {
            bool status = results.value("persistenceStatus").toBool();
            
            if (status) {
                qCDebug(workflowOrchestrator) << "Persistence results validated: data successfully persisted";
                return validatedResults;
            }
        }
        qCWarning(workflowOrchestrator) << "Invalid persistence results - persistence failed or missing status";
        return QVariantMap();
        
    default:
        qCWarning(workflowOrchestrator) << "Unknown stage type for validation:" << static_cast<int>(stage);
        return validatedResults; // Return as-is for unknown stages
    }
}

bool WorkflowOrchestrator::isResultsOptional(StageBasedTaskManager::ProcessingStage stage) const
{
    switch (stage) {
    case StageBasedTaskManager::ProcessingStage::DataPersistence:
        // Persistence stage results are optional - success/failure is more important
        return true;
    case StageBasedTaskManager::ProcessingStage::Search:
        // Search results can be empty (no matches found)
        return true;
    default:
        // Other stages must produce valid results
        return false;
    }
}

QVariantMap WorkflowOrchestrator::getStageResultsFromPrevious(int stageIndex, const QString &sourceStage) const
{
    QVariantMap results;
    
    // Find the source stage results
    for (int i = 0; i < stageIndex; ++i) {
        const WorkflowStage &stage = m_stages[i];
        
        if (!stage.isCompleted) {
            continue;
        }
        
        // Match by stage type name
        QString stageName;
        switch (stage.stage) {
        case StageBasedTaskManager::ProcessingStage::DataParsing:
            stageName = "data_parsing";
            break;
        case StageBasedTaskManager::ProcessingStage::EmbeddingGeneration:
            stageName = "embedding_generation";
            break;
        case StageBasedTaskManager::ProcessingStage::IndexBuilding:
            stageName = "index_building";
            break;
        case StageBasedTaskManager::ProcessingStage::Search:
            stageName = "search";
            break;
        case StageBasedTaskManager::ProcessingStage::DataPersistence:
            stageName = "data_persistence";
            break;
        default:
            continue;
        }
        
        if (stageName == sourceStage) {
            results = stage.results;
            qCDebug(workflowOrchestrator) << "Found results from" << sourceStage << "stage:" << results.keys();
            break;
        }
    }
    
    return results;
}
