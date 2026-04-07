// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "functioncallingsession.h"
#include "sessionmanager/sessionmanager.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelcenter.h"

#include <QDBusConnection>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QThread>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

FunctionCallingSession::FunctionCallingSession(const QString &sessionId, SessionManager *manager, QObject *parent)
    : SessionBase(sessionId, manager, parent)
{
    qCDebug(logDBusServer) << "FunctionCallingSession created:" << sessionId;
}

FunctionCallingSession::~FunctionCallingSession()
{
    cleanup();
    qCDebug(logDBusServer) << "FunctionCallingSession destroyed:" << sessionId;
}

bool FunctionCallingSession::initialize()
{
    // Simple initialization - no complex setup needed
    qCInfo(logDBusServer) << "FunctionCallingSession initialized:" << sessionId;
    return true;
}

void FunctionCallingSession::cleanup()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    generating = false;
    pendingMessages.clear();
    
    qCInfo(logDBusServer) << "FunctionCallingSession cleaned up:" << sessionId;
}

void FunctionCallingSession::Terminate()
{
    QMutexLocker locker(&stateMutex);
    
    if (!generating) {
        qCDebug(logDBusServer) << "No active function calling to terminate for session:" << sessionId;
        return;
    }
    
    // Cancel all active tasks
    auto* taskManager = getTaskManager();
    if (taskManager) {
        QMutexLocker taskLocker(&taskMutex);
        for (quint64 taskId : activeTasks) {
            taskManager->cancelTask(taskId);
            qCInfo(logDBusServer) << "Cancelled task:" << taskId << "for session:" << sessionId;
        }
    }
    
    generating = false;
    pendingMessages.clear();
    
    qCInfo(logDBusServer) << "FunctionCallingSession terminated:" << sessionId;
}

QString FunctionCallingSession::Parse(const QString &content, const QString &functions, const QString &params)
{
    if (content.isEmpty()) {
        qCWarning(logDBusServer) << "Empty function calling content received for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Empty content"})";
    }
    
    if (functions.isEmpty()) {
        qCWarning(logDBusServer) << "Empty function definitions received for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Empty function definitions"})";
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (generating) {
        qCWarning(logDBusServer) << "Function calling request rejected: Generation in progress for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Function calling in progress."})";
    }
    
    // Validate function definitions
    QString validationError;
    if (!validateFunctionDefinitions(functions, validationError)) {
        qCWarning(logDBusServer) << "Invalid function definitions for session:" << sessionId << validationError;
        return QString(R"({"error":1,"errorMessage":"Invalid function definitions: %1"})").arg(validationError);
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create function calling task
    auto funcTask = createFunctionCallingTask(content, functions, varParams);
    
    // Store D-Bus message for delayed reply
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[funcTask.taskId] = msg;
    
    generating = true;
    
    // Submit task to TaskManager
    submitTask(funcTask);
    
    qCInfo(logDBusServer) << "Function calling task submitted:" << funcTask.taskId 
                          << "for session:" << sessionId;
    
    return ""; // Delayed reply
}

void FunctionCallingSession::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    handleFunctionCallingResult(taskId, true, result);
}

void FunctionCallingSession::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    Q_UNUSED(errorCode)
    handleFunctionCallingResult(taskId, false, QVariant(errorMessage));
}

TaskManager::Task FunctionCallingSession::createFunctionCallingTask(const QString &content, const QString &functions, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = AITypes::ServiceType::FunctionCalling;
    task.description = QString("Function calling request for session: %1").arg(sessionId);
    task.metadata["sessionId"] = sessionId;
    task.metadata["contentLength"] = content.length();
    task.metadata["functionsLength"] = functions.length();
    
    // Extract provider and model from params for task management
    task.providerName = params.value("provider", "unknown").toString();
    task.modelName = params.value("model", "unknown").toString();
    
    // Parse function definitions before lambda to avoid accessing this pointer in async context
    QJsonArray funcArray = parseFunctionDefinitions(functions);
    
    // Create task executor that follows the same pattern as TextChatSession
    task.executor = [content, params, taskId = task.taskId, sessionId = this->sessionId, sessionManager = this->sessionManager, funcArray]() {
        try {
            ModelCenter* modelCenter = nullptr;
            if (sessionManager) {
                modelCenter = sessionManager->getModelCenter();
            }
            
            if (!modelCenter) {
                throw std::runtime_error("ModelCenter not available");
            }
            
            // Convert QVariantMap to QVariantHash for ModelCenter
            QVariantHash varParams;
            for (auto it = params.begin(); it != params.end(); ++it) {
                varParams.insert(it.key(), it.value());
            }
            
            // Call ModelCenter functionCalling method
            auto result = modelCenter->functionCalling(sessionId, content, funcArray, varParams);
            
            // Handle result
            QVariantHash out;
            if (result.first == kModelCenterSuccess) {
                // Parse the function result JSON
                QJsonDocument doc = QJsonDocument::fromJson(result.second.toUtf8());
                out.insert("function", doc.object().toVariantHash());
                qCDebug(logDBusServer) << "Function calling completed successfully for session:" << sessionId;
            } else {
                out.insert("error", result.first);
                out.insert("errorMessage", result.second);
                qCWarning(logDBusServer) << "Function calling failed for session:" << sessionId
                                        << "error:" << result.first
                                        << "message:" << result.second;
                
                // Convert error codes to appropriate exceptions for responsive throttling
                if (result.first == kModelCenterErrorRateLimit) {
                    throw NetworkException(kModelCenterErrorRateLimit, result.second);
                } else if (result.first >= kModelCenterErrorServer) {
                    throw NetworkException(result.first, result.second);
                } else if (result.first > kModelCenterSuccess) { // Other errors
                    throw AIServiceException(result.first, result.second);
                }
            }
            
            // Return result for TaskManager to handle
            if (result.first == kModelCenterSuccess) {
                QJsonObject obj = QJsonObject::fromVariantHash(out);
                QJsonDocument doc = QJsonDocument(obj);
                QString resultJson = doc.toJson(QJsonDocument::Compact);
                return QVariant(resultJson);
            } else {
                // For errors, throw exception to let TaskManager handle it
                throw AIServiceException(result.first, result.second);
            }
            
        } catch (const std::exception& e) {
            qCCritical(logDBusServer) << "Exception in function calling task executor:" << e.what();
            
            // Re-throw to let TaskManager handle it
            throw;
        }
    };
    
    return task;
}

void FunctionCallingSession::complete()
{
    QMutexLocker locker(&stateMutex);
    completeInternal();
}

void FunctionCallingSession::completeInternal()
{
    // Internal version without locking - assumes caller holds stateMutex
    generating = false;
    qCDebug(logDBusServer) << "FunctionCallingSession completed:" << sessionId;
}

void FunctionCallingSession::handleFunctionCallingResult(quint64 taskId, bool success, const QVariant& result)
{
    QMutexLocker locker(&stateMutex);
    
    if (success) {
        QString response = result.toString();
        
        // Send D-Bus reply
        auto msgIt = pendingMessages.find(taskId);
        if (msgIt != pendingMessages.end()) {
            sendFunctionCallingResponse(msgIt.value(), response);
            pendingMessages.erase(msgIt);
        }
        
        emit FunctionResult(response);
        qCInfo(logDBusServer) << "Function calling completed for session:" << sessionId;
    } else {
        QString errorMsg = result.toString();
        
        auto msgIt = pendingMessages.find(taskId);
        if (msgIt != pendingMessages.end()) {
            sendFunctionCallingError(msgIt.value(), -1, errorMsg);
            pendingMessages.erase(msgIt);
        }
        
        emit ParseError(-1, errorMsg);
        qCWarning(logDBusServer) << "Function calling failed for session:" << sessionId << errorMsg;
    }
    
    // Clean up
    completeInternal(); // Use internal version since we already hold stateMutex
}

void FunctionCallingSession::sendFunctionCallingResponse(const QDBusMessage& message, const QString& response)
{
    auto connection = QDBusConnection::sessionBus();
    
    // Parse the response to ensure it's valid JSON
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (doc.isNull()) {
        // If response is not JSON, wrap it
        QJsonObject wrapper;
        wrapper["function"] = response;
        doc = QJsonDocument(wrapper);
    }
    
    connection.send(message.createReply(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))));
}

void FunctionCallingSession::sendFunctionCallingError(const QDBusMessage& message, int errorCode, const QString& errorMsg)
{
    auto connection = QDBusConnection::sessionBus();
    
    QJsonObject errorObj;
    errorObj["error"] = errorCode;
    errorObj["errorMessage"] = errorMsg;
    
    QJsonDocument doc(errorObj);
    connection.send(message.createReply(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))));
}

bool FunctionCallingSession::validateFunctionDefinitions(const QString& functions, QString& errorMsg)
{
    if (functions.trimmed().isEmpty()) {
        errorMsg = "Function definitions cannot be empty";
        return false;
    }
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(functions.toUtf8(), &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = QString("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }
    
    if (!doc.isArray()) {
        errorMsg = "Function definitions must be a JSON array";
        return false;
    }
    
    QJsonArray funcArray = doc.array();
    if (funcArray.isEmpty()) {
        errorMsg = "Function definitions array cannot be empty";
        return false;
    }
    
    // Validate each function definition
    for (int i = 0; i < funcArray.size(); ++i) {
        QJsonValue funcValue = funcArray[i];
        if (!funcValue.isObject()) {
            errorMsg = QString("Function definition at index %1 must be an object").arg(i);
            return false;
        }
        
        QJsonObject funcObj = funcValue.toObject();
        if (!funcObj.contains("name") || !funcObj["name"].isString()) {
            errorMsg = QString("Function definition at index %1 must have a 'name' field").arg(i);
            return false;
        }
        
        QString funcName = funcObj["name"].toString();
        if (funcName.trimmed().isEmpty()) {
            errorMsg = QString("Function name at index %1 cannot be empty").arg(i);
            return false;
        }
    }
    
    return true;
}

QJsonArray FunctionCallingSession::parseFunctionDefinitions(const QString& functions)
{
    QJsonDocument doc = QJsonDocument::fromJson(functions.toUtf8());
    return doc.array();
} 
