// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textchatsession.h"
#include "sessionmanager/sessionmanager.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelutils.h"
#include "modelcenter/modelcenter.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QThread>
#include <QPointer>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

TextChatSession::TextChatSession(const QString &sessionId, SessionManager *manager, QObject *parent)
    : SessionBase(sessionId, manager, parent)
{
    qCDebug(logDBusServer) << "TextChatSession created:" << sessionId;
}

TextChatSession::~TextChatSession()
{
    qCDebug(logDBusServer) << "TextChatSession destroyed:" << sessionId;
}

bool TextChatSession::initialize()
{
    // Simple initialization - no complex setup needed
    qCInfo(logDBusServer) << "TextChatSession initialized:" << sessionId;
    return true;
}

void TextChatSession::cleanup()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    generating = false;
    pendingMessages.clear();
    streamingTasks.clear();
    
    qCInfo(logDBusServer) << "TextChatSession cleaned up:" << sessionId;
}

void TextChatSession::terminate()
{
    QMutexLocker locker(&stateMutex);
    
    if (!generating) {
        qCDebug(logDBusServer) << "No active generation to terminate for session:" << sessionId;
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
    streamingTasks.clear();
    
    qCInfo(logDBusServer) << "TextChatSession terminated:" << sessionId;
}

QString TextChatSession::chat(const QString &content, const QString &params)
{
    if (content.isEmpty()) {
        qCWarning(logDBusServer) << "Empty chat content received for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Empty content"})";
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (generating) {
        qCWarning(logDBusServer) << "Chat request rejected: Generation in progress for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Generating in progress."})";
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create chat task
    auto chatTask = createChatTask(content, varParams, false);
    
    // Store D-Bus message for delayed reply
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[chatTask.taskId] = msg;
    streamingTasks[chatTask.taskId] = false;
    
    generating = true;
    
    // Submit task to TaskManager
    submitTask(chatTask);
    
    qCInfo(logDBusServer) << "Chat task submitted:" << chatTask.taskId 
                          << "for session:" << sessionId;
    
    return ""; // Delayed reply
}

int TextChatSession::streamChat(const QString &content, const QString &params)
{
    if (content.isEmpty()) {
        qCWarning(logDBusServer) << "Empty stream chat content received for session:" << sessionId;
        return -1;
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (generating) {
        qCWarning(logDBusServer) << "Stream chat request rejected: Generation in progress for session:" << sessionId;
        return 1;
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create streaming chat task
    auto chatTask = createChatTask(content, varParams, true);
    
    // Store task as streaming
    streamingTasks[chatTask.taskId] = true;
    
    generating = true;
    
    // Submit task to TaskManager
    submitTask(chatTask);
    
    qCInfo(logDBusServer) << "Stream chat task submitted:" << chatTask.taskId 
                          << "for session:" << sessionId;
    
    return 0; // Success
}

void TextChatSession::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    handleChatResult(taskId, true, result);
}

void TextChatSession::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    Q_UNUSED(errorCode)
    handleChatResult(taskId, false, errorMessage);
}

TaskManager::Task TextChatSession::createChatTask(const QString &content, const QVariantMap &params, bool stream)
{
    TaskManager::Task task;
    task.taskType = AITypes::ServiceType::Chat;
    task.description = QString("Chat request for session: %1").arg(sessionId);
    task.metadata["sessionId"] = sessionId;
    task.metadata["stream"] = stream;
    task.metadata["contentLength"] = content.length();
    
    // Extract provider and model information if available
    task.providerName = params.value("provider", "unknown").toString();
    task.modelName = params.value("model", "unknown").toString();
    
    // Extract history before lambda to avoid accessing this pointer in async context
    QList<ChatHistory> history = extractHistoryFromParams(params);
    
    // Create QPointer before lambda to ensure safe capture for streaming
    QPointer<TextChatSession> safeThis(this);
    
    // Task executor - simplified without direct model center access
    task.executor = [content, params, stream, taskId = task.taskId, sessionId = this->sessionId, sessionManager = this->sessionManager, history, safeThis]() {
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
            
            // Set up streamer for streaming responses
            ChatStreamer streamer = nullptr;
            if (stream) {
                streamer = [safeThis, taskId](const QString &text, void *user) -> bool {
                    Q_UNUSED(user)
                    
                    // Check if object is still valid before making async call
                    if (!safeThis) {
                        qCWarning(logDBusServer) << "TextChatSession object destroyed during streaming for task:" << taskId;
                        return false; // Stop streaming
                    }
                    
                    // Send stream output signal with safe object check
                    QMetaObject::invokeMethod(safeThis.data(), "StreamOutput", 
                                            Qt::QueuedConnection, 
                                            Q_ARG(QString, text));
                    
                    return true; // Continue streaming
                };
            }
            
            // Call ModelCenter chat method
            auto result = modelCenter->chat(sessionId, content, history, varParams, streamer, nullptr);
            
            // Handle result
            QVariantHash out;
            if (result.first == kModelCenterSuccess) {
                out.insert("content", result.second);
                qCDebug(logDBusServer) << "Chat request completed successfully for session:" << sessionId;
            } else {
                out.insert("error", result.first);
                out.insert("errorMessage", result.second);
                qCWarning(logDBusServer) << "Chat request failed for session:" << sessionId
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
                QString resultContent = result.second;
                return QVariant(resultContent);
            } else {
                // For errors, throw exception to let TaskManager handle it
                throw AIServiceException(result.first, result.second);
            }
            
        } catch (const std::exception& e) {
            qCCritical(logDBusServer) << "Exception in chat task executor:" << e.what();
            
            // Re-throw to let TaskManager handle it
            throw;
        }
    };
    
    return task;
}

QList<ChatHistory> TextChatSession::extractHistoryFromParams(const QVariantMap &params) const
{
    QList<ChatHistory> history;
    
    // Extract messages from parameters
    QVariant messagesVar = params.value("messages");
    if (messagesVar.isValid()) {
        QVariantList messagesList = messagesVar.toList();
        history = ModelUtils::jsonToHistory(messagesList);
    }
    
    return history;
}

void TextChatSession::complete()
{
    QMutexLocker locker(&stateMutex);
    completeInternal();
}

void TextChatSession::completeInternal()
{
    // Internal version without locking - assumes caller holds stateMutex
    generating = false;
    qCDebug(logDBusServer) << "TextChatSession completed:" << sessionId;
}

void TextChatSession::handleChatResult(quint64 taskId, bool success, const QVariant& result)
{
    QMutexLocker locker(&stateMutex);
    
    // Check if this is a streaming task
    bool isStreaming = streamingTasks.value(taskId, false);
    
    if (success) {
        QString response = result.toString();
        
        if (isStreaming) {
            // For streaming, only emit StreamFinished (StreamOutput was handled in executor)
            emit StreamFinished(0, response);
            qCInfo(logDBusServer) << "Stream chat completed for session:" << sessionId;
        } else {
            // For regular chat, send D-Bus reply
            auto msgIt = pendingMessages.find(taskId);
            if (msgIt != pendingMessages.end()) {
                sendChatResponse(msgIt.value(), response);
                pendingMessages.erase(msgIt);
            }
            qCInfo(logDBusServer) << "Chat completed for session:" << sessionId;
        }
    } else {
        QString errorMsg = result.toString();
        
        if (isStreaming) {
            emit StreamFinished(-1, errorMsg);
            qCWarning(logDBusServer) << "Stream chat failed for session:" << sessionId << errorMsg;
        } else {
            auto msgIt = pendingMessages.find(taskId);
            if (msgIt != pendingMessages.end()) {
                sendChatError(msgIt.value(), -1, errorMsg);
                pendingMessages.erase(msgIt);
            }
            qCWarning(logDBusServer) << "Chat failed for session:" << sessionId << errorMsg;
        }
    }
    
    // Clean up
    streamingTasks.remove(taskId);
    completeInternal(); // Use internal version since we already hold stateMutex
    
    // Call parent implementation for task cleanup
    // Task completion is now handled by SessionBase automatically
}

void TextChatSession::sendChatResponse(const QDBusMessage& message, const QString& response)
{
    QVariantHash out;
    out.insert("content", response);
    
    QJsonObject obj = QJsonObject::fromVariantHash(out);
    QJsonDocument doc = QJsonDocument(obj);
    
    auto connection = QDBusConnection::sessionBus();
    connection.send(message.createReply(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))));
}

void TextChatSession::sendChatError(const QDBusMessage& message, int errorCode, const QString& errorMsg)
{
    QVariantHash out;
    out.insert("error", errorCode);
    out.insert("errorMessage", errorMsg);
    
    QJsonObject obj = QJsonObject::fromVariantHash(out);
    QJsonDocument doc = QJsonDocument(obj);
    
    auto connection = QDBusConnection::sessionBus();
    connection.send(message.createReply(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))));
} 
