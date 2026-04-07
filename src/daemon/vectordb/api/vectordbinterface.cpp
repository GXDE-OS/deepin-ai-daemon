// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vectordbinterface.h"
#include "session/sessionmanager.h"
#include "session/session.h"
#include "models/modelrepository.h"
#include "models/embeddingmodel.h"
#include "vectordb_define.h"

#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFileInfo>
#include <QDBusConnection>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(vectorDbInterface, "vectordb.dbus.interface")

VectorDbInterface::VectorDbInterface(QObject *parent)
    : QObject(parent)
    , m_sessionManager(SessionManager::instance())
{
    // Register QDBusMessage as metatype for QMetaObject::invokeMethod
    qRegisterMetaType<QDBusMessage>("QDBusMessage");
}

VectorDbInterface::~VectorDbInterface()
{

}

// ========== Project Management Operations (Internal) ==========
// Note: Project management is handled internally based on appId + modelId
// External callers only need to specify appId and modelId

// ========== Service Configuration Operations ==========

bool VectorDbInterface::setGlobalConfig(const QString &key, const QVariant &value)
{
    qCDebug(vectorDbInterface) << "Setting global config:" << key << "=" << value;
    
    if (key.isEmpty()) {
        qCWarning(vectorDbInterface) << "Empty key for setGlobalConfig";
        return "";
    }
    
    // TODO: Implement global config setting logic
    return false;
}

QVariant VectorDbInterface::getGlobalConfig(const QString &key)
{
    qCDebug(vectorDbInterface) << "Getting global config:" << key;
    
    if (key.isEmpty()) {
        qCWarning(vectorDbInterface) << "Empty key for getGlobalConfig";
        return QVariant();
    }
    
    // TODO: Implement global config getting logic
    return QVariant();
}

QString VectorDbInterface::getAllConfigs()
{
    qCDebug(vectorDbInterface) << "Getting all global configs";
    
    // TODO: Implement get all configs logic
    return QString();
}

bool VectorDbInterface::resetToDefaults()
{
    qCDebug(vectorDbInterface) << "Resetting configs to defaults";
    
    // TODO: Implement reset to defaults logic
    return false;
}

bool VectorDbInterface::setResourceLimit(const QString &resource, int limit)
{
    qCDebug(vectorDbInterface) << "Setting resource limit:" << resource << "=" << limit;
    
    if (resource.isEmpty() || limit < 0) {
        qCWarning(vectorDbInterface) << "Invalid parameters for setResourceLimit";
        return false;
    }
    
    // TODO: Implement resource limit setting logic
    return true;
}

int VectorDbInterface::getResourceLimit(const QString &resource)
{
    qCDebug(vectorDbInterface) << "Getting resource limit:" << resource;
    
    if (resource.isEmpty()) {
        qCWarning(vectorDbInterface) << "Empty resource for getResourceLimit";
        return -1;
    }
    
    // TODO: Implement resource limit getting logic
    return -1;
}

QString VectorDbInterface::getSystemStatus()
{
    qCDebug(vectorDbInterface) << "Getting system status";
    
    // TODO: Implement system status logic
    QJsonObject status;
    status["isHealthy"] = true;
    status["activeSessionCount"] = m_sessionManager->getActiveSessionCount();
    status["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QJsonDocument(status).toJson(QJsonDocument::Compact);
}

// ========== Vector Database Operations ==========

QString VectorDbInterface::buildIndex(const QString &appId, const QString &docId, const QString &extensionParams, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Building index for app:" << appId;
    
    if (!validateAppId(appId)) {
        qCWarning(vectorDbInterface) << "Invalid parameters for buildIndex";
        // Send error reply if message is valid
        reply->replyError(QDBusError::InvalidArgs, "Invalid parameters for buildIndex");
        return QString();
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        // Send error reply if message is valid
        reply->replyError(QDBusError::InternalError, "Failed to get or create session");
        return QString();
    } else {
        disconnect(session, &Session::indexBuildCompleted, this, nullptr);
        connect(session, &Session::indexBuildCompleted, this, [appId, this](const QString &projectId, const QString &taskId) {
            emit indexBuildCompleted(appId, taskId, "");
        });
    }
        
    // Parse JSON extension parameters
    QJsonDocument doc = QJsonDocument::fromJson(extensionParams.toUtf8());
    QVariantHash params = doc.object().toVariantHash();
    
    // Build index through session
    QString taskId = session->buildIndex(docId, params);
    if (taskId.isEmpty()) {
        qCWarning(vectorDbInterface) << "Failed to build index for app:" << appId;
        // Send error reply if message is valid
        reply->replyError(QDBusError::InternalError, "Failed to build index");
        return QString();
    }
    
    // Build JSON response with taskId
    QJsonObject response;
    response["taskId"] = taskId;
    QString jsonResponse = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply with JSON response if message is valid
    reply->replyString(jsonResponse);
    
    // Emit signal
    emit indexBuildStarted(appId, taskId);
    
    qCDebug(vectorDbInterface) << "Index build started successfully, taskId:" << taskId << "for app:" << appId;
    return jsonResponse;
}

QString VectorDbInterface::uploadDocuments(const QString &appId, const QStringList &files, const QString &extensionParams, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Adding files for app:" << appId << "files:" << files.size();
    
    if (!validateAppId(appId)) {
        qCWarning(vectorDbInterface) << "Invalid parameters for uploadDocuments";
        // Send error reply
        reply->replyError(QDBusError::InvalidArgs, "Invalid parameters for uploadDocuments");
        return "";
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to get or create session");
        return "";
    }

    // Parse JSON extension parameters
    QJsonDocument doc = QJsonDocument::fromJson(extensionParams.toUtf8());
    QVariantHash params = doc.object().toVariantHash();
    
    QList<QVariantHash> uploadResults = session->uploadDocuments(files, params);
    if (uploadResults.isEmpty()) {
        qCWarning(vectorDbInterface) << "Failed to add files for app:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to add files");
        return "";
    }
    
    QStringList ids;
    // Build JSON response array from QVariantHash results
    QJsonArray resultsArray;
    for (const QVariantHash &result : uploadResults) {
        QJsonObject jsonResult;
        jsonResult["file"] = result["file"].toString();
        jsonResult["documentID"] = result["documentID"].toString();
        jsonResult["status"] = result["status"].toInt();
        jsonResult["file_path"] = result["file_path"].toString();
        resultsArray.append(jsonResult);

        ids.append(result["documentID"].toString());
    }
    
    QJsonObject response;
    response["results"] = resultsArray;
    QString jsonResponse = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply with JSON results
    reply->replyString(jsonResponse);
    
    emit documentsAdded(appId, files, ids);
    return jsonResponse;
}

QString VectorDbInterface::deleteDocuments(const QString &appId, const QStringList &documentIds, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Deleting documents for app:" << appId << "documents:" << documentIds.size();
    
    if (!validateAppId(appId) || documentIds.isEmpty()) {
        qCWarning(vectorDbInterface) << "Invalid parameters for deleteDocuments";
        // Send error reply
        reply->replyError(QDBusError::InvalidArgs, "Invalid parameters for deleteDocuments");
        return "";
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to get or create session");
        return "";
    }
    
    // Delete documents through session
    QList<QVariantHash> deleteResults = session->deleteDocuments(documentIds);
    
    QStringList filePaths;
    QJsonArray resultsArray;
    for (const QVariantHash &result : deleteResults) {
        QJsonObject jsonResult;
        jsonResult["documentID"] = result["documentID"].toString();
        jsonResult["status"] = result["status"].toInt();
        jsonResult["file_path"] = result["file_path"].toString();
        resultsArray.append(jsonResult);

        filePaths.append(result["file_path"].toString());
    }
    
    QJsonObject response;
    response["results"] = resultsArray;
    QString jsonResponse = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply
    reply->replyString(jsonResponse);
    
    emit documentsDeleted(appId, documentIds, filePaths);
    
    qCDebug(vectorDbInterface) << "Documents deleted successfully for app:" << appId;
    return jsonResponse;
}

QString VectorDbInterface::destroyIndex(const QString &appId, bool allIndex, const QString &extensionParams, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Deleting index for app:" << appId;
    
    if (!validateAppId(appId)) {
        qCWarning(vectorDbInterface) << "Invalid parameters for deleteIndex";
        // Send error reply
        reply->replyError(QDBusError::InvalidArgs, "Invalid parameters for destroyIndex");
        return "";
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to get or create session");
        return "";
    }
    
    // Parse JSON extension parameters
    QJsonDocument doc = QJsonDocument::fromJson(extensionParams.toUtf8());
    QVariantHash params = doc.object().toVariantHash();

    // Delete index through session
    bool success = session->destroyIndex(allIndex, params);
    if (!success) {
        qCWarning(vectorDbInterface) << "Failed to delete index for app:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to delete index");
        return "";
    }
    
    // Build success response
    QJsonObject response;
    response["success"] = true;
    response["appId"] = appId;
    QString jsonResponse = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply
    reply->replyString(jsonResponse);
        
    qCDebug(vectorDbInterface) << "Index deleted successfully for app:" << appId;
    return jsonResponse;
}

QString VectorDbInterface::search(const QString &appId, const QString &query, const QString &extensionParams, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Searching for app:" << appId << "query:" << query;

    if (!validateAppId(appId) || query.isEmpty()) {
        qCWarning(vectorDbInterface) << "Invalid parameters for search";
        // Send error reply
        reply->replyError(QDBusError::InvalidArgs, "Invalid parameters for search");
        return "";
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to get or create session");
        return "";
    }
    
    // Parse JSON extension parameters
    QJsonDocument doc = QJsonDocument::fromJson(extensionParams.toUtf8());
    QVariantHash params = doc.object().toVariantHash();
    
    // Perform search through session
    QVariantList results = session->search(query, params);
    if (results.isEmpty()) {
        qCDebug(vectorDbInterface) << "No search results found for query:" << query << "in app:" << appId;
    }
    
    // Convert results to JSON string
    QJsonArray jsonArray;
    for (const QVariant &result : results) {
        QVariantMap resultMap = result.toMap();
        QJsonObject jsonResult = QJsonObject::fromVariantMap(resultMap);
        jsonArray.append(jsonResult);
    }
    
    QJsonObject response;
    response["results"] = jsonArray;
    QString jsonResults = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply with results
    reply->replyString(jsonResults);
    
    // Emit signal
    emit searchCompleted(appId, jsonResults);
    
    qCDebug(vectorDbInterface) << "Search completed, found" << results.size() << "results for app:" << appId;
    return jsonResults;
}

QString VectorDbInterface::searchAsync(const QString &appId, const QString &query, const QString &extensionParams)
{
    qCDebug(vectorDbInterface) << "Async search for app:" << appId << "query:" << query;
    
    if (!validateAppId(appId) || query.isEmpty()) {
        qCWarning(vectorDbInterface) << "Invalid parameters for searchAsync";
        return QString();
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        return QString();
    }
    
    // Parse JSON extension parameters
    QJsonDocument doc = QJsonDocument::fromJson(extensionParams.toUtf8());
    QVariantHash params = doc.object().toVariantHash();
    
    // Perform async search through session
    QString taskId = session->searchAsync(query, params);
    if (taskId.isEmpty()) {
        qCWarning(vectorDbInterface) << "Failed to start async search for app:" << appId;
        return QString();
    }
    
    // Emit signal
    emit searchStarted(appId, taskId);
    
    qCDebug(vectorDbInterface) << "Async search started successfully, taskId:" << taskId << "for app:" << appId;
    return taskId;
}

QString VectorDbInterface::getIndexStatus(const QString &appId)
{
    qCDebug(vectorDbInterface) << "Getting index status for app:" << appId;
    
    if (!validateAppId(appId)) {
        qCWarning(vectorDbInterface) << "Invalid parameters for getIndexStatus";
        return QString();
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        return QString();
    }
    
    // Get index status through session
    QVariantMap status = session->getIndexStatus();
    if (status.isEmpty()) {
        qCDebug(vectorDbInterface) << "No index status available for app:" << appId;
        // Return basic status structure
        status["appId"] = appId;
        status["isReady"] = false;
        status["documentCount"] = 0;
        status["vectorCount"] = 0;
        status["status"] = "not_initialized";
        status["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    }
    
    // Convert to JSON string
    QJsonObject jsonStatus = QJsonObject::fromVariantMap(status);
    QString jsonString = QJsonDocument(jsonStatus).toJson(QJsonDocument::Compact);
    
    qCDebug(vectorDbInterface) << "Index status retrieved for app:" << appId;
    return jsonString;
}

QString VectorDbInterface::getTaskProgress(const QString &taskId)
{
    qCDebug(vectorDbInterface) << "Getting task progress:" << taskId;
    
    if (!validateTaskId(taskId)) {
        qCWarning(vectorDbInterface) << "Invalid taskId for getTaskProgress";
        return QString();
    }
    
    // Task progress needs to be searched across all sessions since we don't know which session owns the task
    // In a production implementation, we would maintain a global task registry in SessionManager
    // For now, we'll iterate through active sessions to find the task
    
    QStringList activeSessions = m_sessionManager->getActiveSessionIds();
    for (const QString &appId : activeSessions) {
        Session *session = m_sessionManager->getSession(appId);
        if (session) {
            // Check if this session has the task
            // In a full implementation, Session would have a getTaskProgress method
            // For now, we'll return a basic progress structure
            QJsonObject progress;
            progress["taskId"] = taskId;
            progress["appId"] = appId;
            progress["status"] = "running";
            progress["progress"] = 50; // Mock progress
            progress["stage"] = "processing";
            progress["startTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            progress["estimatedCompletion"] = QDateTime::currentDateTime().addSecs(300).toString(Qt::ISODate);
            
            qCDebug(vectorDbInterface) << "Task progress retrieved for taskId:" << taskId;
            return QJsonDocument(progress).toJson(QJsonDocument::Compact);
        }
    }
    
    // Task not found in any session
    qCWarning(vectorDbInterface) << "Task not found:" << taskId;
    QJsonObject progress;
    progress["taskId"] = taskId;
    progress["status"] = "not_found";
    progress["error"] = "Task not found or has been completed";
    
    return QJsonDocument(progress).toJson(QJsonDocument::Compact);
}

bool VectorDbInterface::cancelTask(const QString &taskId, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Cancelling task:" << taskId;
    
    if (!validateTaskId(taskId)) {
        qCWarning(vectorDbInterface) << "Invalid taskId for cancelTask";
        // Send error reply
        reply->replyError(QDBusError::InvalidArgs, "Invalid taskId for cancelTask");
        return false;
    }
    
    // Task cancellation needs to be searched across all sessions since we don't know which session owns the task
    // In a production implementation, we would maintain a global task registry in SessionManager
    
    QStringList activeSessions = m_sessionManager->getActiveSessionIds();
    for (const QString &appId : activeSessions) {
        Session *session = m_sessionManager->getSession(appId);
        if (session) {
            // In a full implementation, we would call session->cancelTask(taskId)
            // For now, we'll simulate task cancellation
            // The actual cancellation would be handled by the VectorDb component within the session
            
            qCDebug(vectorDbInterface) << "Attempting to cancel task:" << taskId << "in session:" << appId;
            
            // Send successful reply
            reply->replyBool(true);
            
            // Emit cancellation signal
            emit taskCancelled(taskId);
            
            qCDebug(vectorDbInterface) << "Task cancellation requested for taskId:" << taskId;
            return true;
        }
    }
    
    // Task not found in any session
    qCWarning(vectorDbInterface) << "Task not found for cancellation:" << taskId;
    // Send error reply
    reply->replyError(QDBusError::InternalError, "Task not found for cancellation");
    return false;
}

QString VectorDbInterface::getWorkflowProgress(const QString &workflowId)
{
    qCDebug(vectorDbInterface) << "Getting workflow progress:" << workflowId;
    
    if (workflowId.isEmpty()) {
        qCWarning(vectorDbInterface) << "Empty workflowId for getWorkflowProgress";
        return QString();
    }
    
    // Workflow progress needs to be searched across all sessions since we don't know which session owns the workflow
    // In a production implementation, we would maintain a global workflow registry in SessionManager
    
    QStringList activeSessions = m_sessionManager->getActiveSessionIds();
    for (const QString &appId : activeSessions) {
        Session *session = m_sessionManager->getSession(appId);
        if (session) {
            // In a full implementation, we would call session->getWorkflowProgress(workflowId)
            // For now, we'll return a basic workflow progress structure
            QJsonObject progress;
            progress["workflowId"] = workflowId;
            progress["appId"] = appId;
            progress["status"] = "running";
            progress["completedStages"] = 2;
            progress["totalStages"] = 5;
            progress["currentStage"] = "embedding";
            progress["overallProgress"] = 40; // (2/5) * 100
            progress["startTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            progress["estimatedCompletion"] = QDateTime::currentDateTime().addSecs(600).toString(Qt::ISODate);
            
            // Stage details
            QJsonArray stages;
            QJsonObject stage1;
            stage1["name"] = "data_parsing";
            stage1["status"] = "completed";
            stage1["progress"] = 100;
            stages.append(stage1);
            
            QJsonObject stage2;
            stage2["name"] = "text_chunking";
            stage2["status"] = "completed";
            stage2["progress"] = 100;
            stages.append(stage2);
            
            QJsonObject stage3;
            stage3["name"] = "embedding_generation";
            stage3["status"] = "running";
            stage3["progress"] = 60;
            stages.append(stage3);
            
            QJsonObject stage4;
            stage4["name"] = "index_building";
            stage4["status"] = "pending";
            stage4["progress"] = 0;
            stages.append(stage4);
            
            QJsonObject stage5;
            stage5["name"] = "data_persistence";
            stage5["status"] = "pending";
            stage5["progress"] = 0;
            stages.append(stage5);
            
            progress["stages"] = stages;
            
            qCDebug(vectorDbInterface) << "Workflow progress retrieved for workflowId:" << workflowId;
            return QJsonDocument(progress).toJson(QJsonDocument::Compact);
        }
    }
    
    // Workflow not found in any session
    qCWarning(vectorDbInterface) << "Workflow not found:" << workflowId;
    QJsonObject progress;
    progress["workflowId"] = workflowId;
    progress["status"] = "not_found";
    progress["error"] = "Workflow not found or has been completed";
    
    return QJsonDocument(progress).toJson(QJsonDocument::Compact);
}

// ========== Helper Methods ==========

Session* VectorDbInterface::getOrCreateSession(const QString &appId)
{
    if (!validateAppId(appId)) {
        qCWarning(vectorDbInterface) << "Invalid appId:" << appId;
        return nullptr;
    }
    
    // First try to get existing session
    Session *session = m_sessionManager->getSession(appId);
    if (session) {
        return session;
    }
    
    // If no session exists, create one
    session = m_sessionManager->createSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to create session for appId:" << appId;
        return nullptr;
    }
    
    qCDebug(vectorDbInterface) << "Created new session for appId:" << appId;
    return session;
}

bool VectorDbInterface::validateAppId(const QString &appId) const
{
    return !appId.isEmpty() && appId.length() <= 128;
}

bool VectorDbInterface::validateTaskId(const QString &taskId) const
{
    return !taskId.isEmpty() && taskId.length() <= 64;
}

bool VectorDbInterface::validateConfig(const QString &config) const
{
    // Basic config validation
    if (config.isEmpty()) return false;
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(config.toUtf8(), &error);
    return error.error == QJsonParseError::NoError;
}

QString VectorDbInterface::documentsInfo(const QString &appId, const QStringList &documentIds, VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Getting documents info for app:" << appId << "documentIds count:" << documentIds.size();
    
    if (!validateAppId(appId)) {
        qCWarning(vectorDbInterface) << "Invalid parameters for documentsInfo";
        // Send error reply
        reply->replyError(QDBusError::InvalidArgs, "Invalid parameters for documentsInfo");
        return QString();
    }
    
    // Get or create session for the app
    Session *session = getOrCreateSession(appId);
    if (!session) {
        qCWarning(vectorDbInterface) << "Failed to get or create session for appId:" << appId;
        // Send error reply
        reply->replyError(QDBusError::InternalError, "Failed to get or create session");
        return QString();
    }
    
    // Get documents info through session
    QVariantList documentsInfoList = session->documentsInfo(documentIds);
    
    // Convert to JSON string
    QJsonArray jsonArray;
    for (const QVariant &item : documentsInfoList) {
        QVariantMap itemMap = item.toMap();
        QJsonObject jsonItem = QJsonObject::fromVariantMap(itemMap);
        jsonArray.append(jsonItem);
    }
    
    QJsonObject response;
    response["results"] = jsonArray;
    QString jsonResult = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply with documents info
    reply->replyString(jsonResult);
    
    qCDebug(vectorDbInterface) << "Documents info retrieved successfully for app:" << appId << "count:" << jsonArray.size();
    return jsonResult;
}

QString VectorDbInterface::embeddingModels(VectorDbReplyPtr reply)
{
    qCDebug(vectorDbInterface) << "Getting available embedding models";
    
    ModelRepository repo;
    repo.refresh();

    // Get model list from repository
    QStringList modelIds = repo.getModelList();
    
    QJsonArray modelsArray;
    for (const QString &modelId : modelIds) {
        QVariantHash modelInfo = repo.getModelInfo(modelId);
        
        QJsonObject modelObj;
        modelObj["model"] = modelId;

        auto parts = repo.fromModelId(modelId);
        if (parts.first.isEmpty() || parts.second.isEmpty()) {
            qCWarning(vectorDbInterface) << "invalid model id" << modelId
                                         << "provider" << parts.first << "model" << parts.second;
            continue;
        }

        modelObj["provider"] = parts.first;
        modelObj["name"] = parts.second;
        modelsArray.append(modelObj);
    }
    
    QJsonObject response;
    response["results"] = modelsArray;
    QString jsonResult = QJsonDocument(response).toJson(QJsonDocument::Compact);
    
    // Send successful reply with models info
    reply->replyString(jsonResult);
    
    qCDebug(vectorDbInterface) << "Retrieved" << modelsArray.size() << "embedding models";
    return jsonResult;
}

QString VectorDbInterface::embeddingTexts(const QString &appId, const QStringList &texts, const QString &extensionParams, VectorDbReplyPtr reply)
{
    // Parse JSON extension parameters
    QJsonDocument doc = QJsonDocument::fromJson(extensionParams.toUtf8());
    QVariantHash params = doc.object().toVariantHash();
    QString modelID = ExtensionParams::getModel(params);
    ModelRepository respo;
    respo.refresh();

    if (modelID.isEmpty()) {
        qCWarning(vectorDbInterface) << "the model id is not set, use default model";
        modelID = respo.defaultModel();
    }

    qCInfo(vectorDbInterface) << "embedding texts with model" << modelID;

    QScopedPointer<EmbeddingModel> embedModel(respo.createModel(modelID, respo.getModelInfo(modelID)));
    if (embedModel.isNull()) {
        qCWarning(vectorDbInterface) << "unknown model id" << modelID;
        reply->replyError(QDBusError::InvalidArgs, "Invalid model id");
        return "";
    }

    QJsonObject response;
    {
        response = embedModel->embeddings(texts);
        response.remove("embeddings");
        response[ExtensionParams::kModelKey] = modelID;
    }

    QString jsonResult = QJsonDocument(response).toJson(QJsonDocument::Compact);
    reply->replyString(jsonResult);
    return jsonResult;
}
