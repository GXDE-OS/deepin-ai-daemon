// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "speechtotextsession.h"
#include "sessionmanager/sessionmanager.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelutils.h"
#include "modelcenter/modelcenter.h"
#include "modelcenter/interfaces/speechtotextinterface.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QThread>
#include <QUuid>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

SpeechToTextSession::SpeechToTextSession(const QString &sessionId, SessionManager *manager, QObject *parent)
    : SessionBase(sessionId, manager, parent)
{
    qCDebug(logDBusServer) << "SpeechToTextSession created:" << sessionId;
}

SpeechToTextSession::~SpeechToTextSession()
{
    qCDebug(logDBusServer) << "SpeechToTextSession destroyed:" << sessionId;
}

bool SpeechToTextSession::initialize()
{    
    qCInfo(logDBusServer) << "SpeechToTextSession initialized:" << sessionId;
    return true;
}

void SpeechToTextSession::cleanup()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    recognizing = false;
    pendingMessages.clear();
    streamingTasks.clear();
    streamSessions.clear();
    
    // Clean up speech interface
    if (currentSpeechInterface) {
        currentSpeechInterface->terminate();
        currentSpeechInterface.reset();
    }
    
    qCInfo(logDBusServer) << "SpeechToTextSession cleaned up:" << sessionId;
}

void SpeechToTextSession::terminate()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    recognizing = false;
    pendingMessages.clear();
    streamingTasks.clear();
    streamSessions.clear();
    
    qCInfo(logDBusServer) << "SpeechToTextSession terminated:" << sessionId;
}

QString SpeechToTextSession::recognizeFile(const QString &audioFile, const QString &params)
{
    if (audioFile.isEmpty()) {
        qCWarning(logDBusServer) << "Empty audio file path received for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Empty audio file path"})";
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (recognizing) {
        qCWarning(logDBusServer) << "Recognition request rejected: Recognition in progress for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Recognition in progress."})";
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create recognition task
    auto recognitionTask = createRecognitionTask(audioFile, varParams);
    
    // Store D-Bus message for delayed reply using the task's own ID
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[recognitionTask.taskId] = msg;
    streamingTasks[recognitionTask.taskId] = false;
    
    recognizing = true;
    
    // Submit task to TaskManager
    submitTask(recognitionTask);
    
    qCInfo(logDBusServer) << "File recognition task submitted:" << recognitionTask.taskId 
                          << "for session:" << sessionId
                          << "file:" << audioFile;
    
    return ""; // Delayed reply
}

QString SpeechToTextSession::startStreamRecognition(const QString &params)
{
    QMutexLocker locker(&stateMutex);
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    try {
        auto* modelCenter = getModelCenter();
        if (!modelCenter) {
            qCWarning(logDBusServer) << "ModelCenter not available for session:" << sessionId;
            return "";
        }
        
        // Convert QVariantMap to QVariantHash for ModelCenter
        QVariantHash varHashParams;
        for (auto it = varParams.begin(); it != varParams.end(); ++it) {
            varHashParams.insert(it.key(), it.value());
        }
        
        // Create provider and interface if not already created
        if (!currentSpeechInterface) {
            auto provider = modelCenter->createProviderForInterface(SpeechToText, varHashParams, sessionId);
            if (!provider) {
                qCWarning(logDBusServer) << "Failed to create SpeechToText provider for session:" << sessionId;
                return "";
            }
            
            currentSpeechInterface = QSharedPointer<SpeechToTextInterface>(
                dynamic_cast<SpeechToTextInterface*>(provider->createInterface(SpeechToText))
            );
            
            if (!currentSpeechInterface) {
                qCWarning(logDBusServer) << "Failed to create SpeechToText interface for session:" << sessionId;
                return "";
            }
            
            // Connect interface signals to handler slots
            connect(currentSpeechInterface.data(), &SpeechToTextInterface::recognitionResult,
                    this, &SpeechToTextSession::onInterfaceRecognitionResult);
            connect(currentSpeechInterface.data(), &SpeechToTextInterface::recognitionPartialResult,
                    this, &SpeechToTextSession::onInterfaceRecognitionPartialResult);
            connect(currentSpeechInterface.data(), &SpeechToTextInterface::recognitionError,
                    this, &SpeechToTextSession::onInterfaceRecognitionError);
            connect(currentSpeechInterface.data(), &SpeechToTextInterface::recognitionCompleted,
                    this, &SpeechToTextSession::onInterfaceRecognitionCompleted);
        }
        
        // Start stream recognition using the interface
        QString streamSessionId = currentSpeechInterface->startStreamRecognition(varHashParams);
        if (streamSessionId.isEmpty()) {
            qCWarning(logDBusServer) << "Failed to start stream recognition for session:" << sessionId;
            return "";
        }
        
        // Create stream session
        StreamSession session;
        session.streamSessionId = streamSessionId;
        session.isActive = true;
        
        streamSessions[streamSessionId] = session;
        
        qCInfo(logDBusServer) << "Started stream recognition session:" << streamSessionId 
                              << "for session:" << sessionId;
        
        return streamSessionId;
        
    } catch (const std::exception& e) {
        qCCritical(logDBusServer) << "Exception in StartStreamRecognition:" << e.what() 
                                  << "for session:" << sessionId;
        return "";
    }
}

bool SpeechToTextSession::sendAudioData(const QString &streamSessionId, const QByteArray &audioData)
{
    QMutexLocker locker(&stateMutex);
    
    if (!streamSessions.contains(streamSessionId)) {
        qCWarning(logDBusServer) << "Invalid stream session ID:" << streamSessionId 
                                 << "for session:" << sessionId;
        return false;
    }
    
    StreamSession &session = streamSessions[streamSessionId];
    if (!session.isActive) {
        qCWarning(logDBusServer) << "Stream session is not active:" << streamSessionId 
                                 << "for session:" << sessionId;
        return false;
    }
    
    // Send audio data to the speech interface
    if (currentSpeechInterface) {
        bool success = currentSpeechInterface->sendAudioData(streamSessionId, audioData);
        if (success) {
            qCDebug(logDBusServer) << "Audio data sent to interface for stream session:" << streamSessionId 
                                   << "size:" << audioData.size() 
                                   << "for session:" << sessionId;
        } else {
            qCWarning(logDBusServer) << "Failed to send audio data to interface for stream session:" << streamSessionId 
                                     << "for session:" << sessionId;
        }
        return success;
    } else {
        qCWarning(logDBusServer) << "No speech interface available for stream session:" << streamSessionId 
                                 << "for session:" << sessionId;
        return false;
    }
}

QString SpeechToTextSession::endStreamRecognition(const QString &streamSessionId)
{
    QMutexLocker locker(&stateMutex);
    
    if (!streamSessions.contains(streamSessionId)) {
        qCWarning(logDBusServer) << "Invalid stream session ID:" << streamSessionId 
                                 << "for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Invalid stream session ID"})";
    }
    
    StreamSession &session = streamSessions[streamSessionId];
    if (!session.isActive) {
        qCWarning(logDBusServer) << "Stream session is not active:" << streamSessionId 
                                 << "for session:" << sessionId;
        return R"({"error":1,"errorMessage":"Stream session not active"})";
    }
    
    // End stream recognition using the speech interface
    if (currentSpeechInterface) {
        try {
            QJsonObject result = currentSpeechInterface->endStreamRecognition(streamSessionId);
            
            // Mark session as inactive
            session.isActive = false;
            
            // Convert result to JSON string
            QString resultJson = QJsonDocument(result).toJson(QJsonDocument::Compact);
            
            qCInfo(logDBusServer) << "Stream recognition ended for session:" << streamSessionId 
                                  << "session:" << sessionId
                                  << "result:" << resultJson;
            
            return resultJson;
            
        } catch (const std::exception& e) {
            qCCritical(logDBusServer) << "Exception in EndStreamRecognition:" << e.what() 
                                      << "for stream session:" << streamSessionId
                                      << "session:" << sessionId;
            
            // Mark session as inactive
            session.isActive = false;
            
            return R"({"error":1,"errorMessage":"Exception in stream recognition"})";
        }
    } else {
        qCWarning(logDBusServer) << "No speech interface available for stream session:" << streamSessionId 
                                 << "for session:" << sessionId;
        
        // Mark session as inactive
        session.isActive = false;
        
        return R"({"error":1,"errorMessage":"No speech interface available"})";
    }
}

QStringList SpeechToTextSession::getSupportedFormats()
{
    // Get supported formats from the current interface
    if (currentSpeechInterface) {
        return currentSpeechInterface->getSupportedFormats();
    }
    
    // If no interface is available, try to create one temporarily to get the supported formats
    try {
        auto* modelCenter = getModelCenter();
        if (modelCenter) {
            QVariantHash params; // Empty params for getting default provider
            auto provider = modelCenter->createProviderForInterface(SpeechToText, params, sessionId);
            if (provider) {
                auto speechInterface = QSharedPointer<SpeechToTextInterface>(
                    dynamic_cast<SpeechToTextInterface*>(provider->createInterface(SpeechToText))
                );
                if (speechInterface) {
                    return speechInterface->getSupportedFormats();
                }
            }
        }
    } catch (const std::exception& e) {
        qCWarning(logDBusServer) << "Failed to get supported formats from interface:" << e.what();
    }

    return {};
}

void SpeechToTextSession::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    handleRecognitionResult(taskId, true, result);
}

void SpeechToTextSession::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    Q_UNUSED(errorCode)
    handleRecognitionResult(taskId, false, errorMessage);
}

TaskManager::Task SpeechToTextSession::createRecognitionTask(const QString &audioFile, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = AITypes::ServiceType::SpeechToText;
    task.description = QString("Speech recognition for session: %1").arg(sessionId);
    task.metadata["sessionId"] = sessionId;
    task.metadata["audioFile"] = audioFile;
    task.metadata["stream"] = false;
    
    // Extract provider and model information if available
    task.providerName = params.value("provider").toString();
    task.modelName = params.value("model").toString();
    
    // Task executor - avoid accessing this pointer in async context
    task.executor = [audioFile, params, taskId = task.taskId, sessionId = this->sessionId, sessionManager = this->sessionManager]() {
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
            
            // Create provider and interface
            auto provider = modelCenter->createProviderForInterface(SpeechToText, varParams, sessionId);
            if (!provider) {
                throw std::runtime_error("Failed to create SpeechToText provider");
            }
            
            auto speechInterface = QSharedPointer<SpeechToTextInterface>(
                dynamic_cast<SpeechToTextInterface*>(provider->createInterface(SpeechToText))
            );
            
            if (!speechInterface) {
                throw std::runtime_error("Failed to create SpeechToText interface");
            }
            
            // Perform recognition
            auto result = speechInterface->recognizeFile(audioFile, varParams);
            
            // Return result
            return QVariant::fromValue(result);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(QString("Recognition failed: %1").arg(e.what()).toStdString());
        }
    };
    
    return task;
}

TaskManager::Task SpeechToTextSession::createStreamRecognitionTask(const QString &streamSessionId, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = AITypes::ServiceType::SpeechToText;
    task.description = QString("Stream speech recognition for session: %1").arg(sessionId);
    task.metadata["sessionId"] = sessionId;
    task.metadata["streamSessionId"] = streamSessionId;
    task.metadata["stream"] = true;
    
    // Extract provider and model information if available
    task.providerName = params.value("provider").toString();
    task.modelName = params.value("model").toString();
    
    // Task executor - avoid accessing this pointer in async context
    task.executor = [streamSessionId, params, taskId = task.taskId, sessionId = this->sessionId, sessionManager = this->sessionManager]() {
        try {
            ModelCenter* modelCenter = nullptr;
            if (sessionManager) {
                modelCenter = sessionManager->getModelCenter();
            }
            
            if (!modelCenter) {
                throw std::runtime_error("ModelCenter not available");
            }
            
            // Get audio data from stream session
            QByteArray audioData = params.value("audio_data").toByteArray();
            
            // Convert QVariantMap to QVariantHash for ModelCenter
            QVariantHash varParams;
            for (auto it = params.begin(); it != params.end(); ++it) {
                varParams.insert(it.key(), it.value());
            }
            
            // Create provider and interface
            auto provider = modelCenter->createProviderForInterface(SpeechToText, varParams, sessionId);
            if (!provider) {
                throw std::runtime_error("Failed to create SpeechToText provider");
            }
            
            auto speechInterface = QSharedPointer<SpeechToTextInterface>(
                dynamic_cast<SpeechToTextInterface*>(provider->createInterface(SpeechToText))
            );
            
            if (!speechInterface) {
                throw std::runtime_error("Failed to create SpeechToText interface");
            }
            
            // For stream recognition, we need to implement a different approach
            // For now, we'll use the file-based approach with temporary data
            // TODO: Implement proper stream recognition
            
            // Create a temporary result
            QJsonObject result;
            result["success"] = true;
            result["error_code"] = 0;
            result["error_message"] = "";
            result["text"] = "Stream recognition not fully implemented yet";
            
            return QVariant::fromValue(result);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(QString("Stream recognition failed: %1").arg(e.what()).toStdString());
        }
    };
    
    return task;
}

void SpeechToTextSession::handleRecognitionResult(quint64 taskId, bool success, const QVariant& result)
{
    QMutexLocker locker(&stateMutex);
    
    // Find pending message
    auto msgIt = pendingMessages.find(taskId);
    if (msgIt == pendingMessages.end()) {
        qCWarning(logDBusServer) << "No pending message found for task:" << taskId;
        return;
    }
    
    QDBusMessage msg = msgIt.value();
    bool isStreaming = streamingTasks.value(taskId, false);
    
    // Clean up
    pendingMessages.erase(msgIt);
    streamingTasks.remove(taskId);
    recognizing = false;
    
    // Send response
    if (success) {
        if (result.canConvert<QJsonObject>()) {
            QJsonObject jsonResult = result.value<QJsonObject>();
            QString response = QJsonDocument(jsonResult).toJson(QJsonDocument::Compact);
            sendRecognitionResponse(msg, response);
        } else {
            QString response = result.toString();
            sendRecognitionResponse(msg, response);
        }
    } else {
        sendRecognitionError(msg, 1, result.toString());
    }
    
    // Clean up stream session if it was a stream task
    if (isStreaming) {
        auto task = getTaskManager()->getTask(taskId);
        QString streamSessionId = task.metadata.value("streamSessionId").toString();
        if (!streamSessionId.isEmpty() && streamSessions.contains(streamSessionId)) {
            streamSessions[streamSessionId].isActive = false;
        }
    }
}

void SpeechToTextSession::sendRecognitionResponse(const QDBusMessage& message, const QString& response)
{
    QDBusMessage reply = message.createReply(response);
    QDBusConnection::sessionBus().send(reply);
}

void SpeechToTextSession::sendRecognitionError(const QDBusMessage& message, int errorCode, const QString& errorMsg)
{
    QJsonObject error;
    error["success"] = false;
    error["error_code"] = errorCode;
    error["error_message"] = errorMsg;
    error["text"] = "";
    
    QString response = QJsonDocument(error).toJson(QJsonDocument::Compact);
    sendRecognitionResponse(message, response);
}

// Stream recognition interface signal handlers
void SpeechToTextSession::onInterfaceRecognitionResult(const QString &sessionId, const QString &text)
{
    qCDebug(logDBusServer) << "Interface recognition result for session:" << sessionId 
                           << "text:" << text << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit RecognitionResult(sessionId, text);
}

void SpeechToTextSession::onInterfaceRecognitionPartialResult(const QString &sessionId, const QString &partialText)
{
    qCDebug(logDBusServer) << "Interface partial result for session:" << sessionId 
                           << "text:" << partialText << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit RecognitionPartialResult(sessionId, partialText);
}

void SpeechToTextSession::onInterfaceRecognitionError(const QString &sessionId, int errorCode, const QString &message)
{
    qCWarning(logDBusServer) << "Interface recognition error for session:" << sessionId 
                             << "error:" << errorCode << message << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit RecognitionError(sessionId, errorCode, message);
}

void SpeechToTextSession::onInterfaceRecognitionCompleted(const QString &sessionId, const QString &finalText)
{
    qCInfo(logDBusServer) << "Interface recognition completed for session:" << sessionId 
                          << "text:" << finalText << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit RecognitionCompleted(sessionId, finalText);
} 
