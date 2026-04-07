// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "texttospeechsession.h"
#include "sessionmanager/sessionmanager.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelutils.h"
#include "modelcenter/modelcenter.h"
#include "modelcenter/interfaces/texttospeechinterface.h"

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

TextToSpeechSession::TextToSpeechSession(const QString &sessionId, SessionManager *manager, QObject *parent)
    : SessionBase(sessionId, manager, parent)
{
    qCDebug(logDBusServer) << "TextToSpeechSession created:" << sessionId;
}

TextToSpeechSession::~TextToSpeechSession()
{
    qCDebug(logDBusServer) << "TextToSpeechSession destroyed:" << sessionId;
}

bool TextToSpeechSession::initialize()
{    
    qCInfo(logDBusServer) << "TextToSpeechSession initialized:" << sessionId;
    return true;
}

void TextToSpeechSession::cleanup()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    synthesizing = false;
    pendingMessages.clear();
    streamingTasks.clear();
    streamSessions.clear();
    
    // Clean up TTS interface
    if (currentTTSInterface) {
        currentTTSInterface->terminate();
        currentTTSInterface.reset();
    }
    
    qCInfo(logDBusServer) << "TextToSpeechSession cleaned up:" << sessionId;
}

void TextToSpeechSession::terminate()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    synthesizing = false;
    pendingMessages.clear();
    streamingTasks.clear();
    streamSessions.clear();
    
    qCInfo(logDBusServer) << "TextToSpeechSession terminated:" << sessionId;
}

QStringList TextToSpeechSession::getSupportedVoices()
{
    // Get supported voices from the current interface
    if (currentTTSInterface) {
        return currentTTSInterface->getSupportedVoices();
    }
    
    // If no interface is available, try to create one temporarily to get the supported voices
    try {
        auto* modelCenter = getModelCenter();
        if (modelCenter) {
            QVariantHash params; // Empty params for getting default provider
            auto provider = modelCenter->createProviderForInterface(TextToSpeech, params, sessionId);
            if (provider) {
                auto ttsInterface = QSharedPointer<TextToSpeechInterface>(
                    dynamic_cast<TextToSpeechInterface*>(provider->createInterface(TextToSpeech))
                );
                if (ttsInterface) {
                    return ttsInterface->getSupportedVoices();
                }
            }
        }
    } catch (const std::exception& e) {
        qCWarning(logDBusServer) << "Failed to get supported voices from interface:" << e.what();
    }

    return {};
}

void TextToSpeechSession::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    handleSynthesisResult(taskId, true, result);
}

void TextToSpeechSession::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    QMutexLocker locker(&stateMutex);
    
    auto msgIt = pendingMessages.find(taskId);
    if (msgIt != pendingMessages.end()) {
        sendSynthesisError(msgIt.value(), errorCode, errorMessage);
        pendingMessages.erase(msgIt);
    }
    
    synthesizing = false;
    
    qCWarning(logDBusServer) << "Synthesis task failed:" << taskId << "error:" << errorCode << errorMessage;
}

void TextToSpeechSession::handleSynthesisResult(quint64 taskId, bool success, const QVariant& result)
{
    QMutexLocker locker(&stateMutex);
    
    auto msgIt = pendingMessages.find(taskId);
    if (msgIt == pendingMessages.end()) {
        qCWarning(logDBusServer) << "No pending message found for task:" << taskId;
        return;
    }
    
    if (success) {
        QJsonObject jsonResult = result.toJsonObject();
        QJsonDocument doc(jsonResult);
        QString response = doc.toJson(QJsonDocument::Compact);
        sendSynthesisResponse(msgIt.value(), response);
    } else {
        sendSynthesisError(msgIt.value(), -1, "Synthesis failed");
    }
    
    pendingMessages.erase(msgIt);
    synthesizing = false;
    
    qCInfo(logDBusServer) << "Handled synthesis result for task:" << taskId << "success:" << success;
}

void TextToSpeechSession::sendSynthesisResponse(const QDBusMessage& message, const QString& response)
{
    QDBusConnection::sessionBus().send(message.createReply(response));
}

void TextToSpeechSession::sendSynthesisError(const QDBusMessage& message, int errorCode, const QString& errorMsg)
{
    QDBusConnection::sessionBus().send(message.createErrorReply("org.deepin.ai.daemon.Error",
                                                               QString("Synthesis failed: %1 (code: %2)").arg(errorMsg).arg(errorCode)));
}

QString TextToSpeechSession::startStreamSynthesis(const QString &text, const QString &params)
{
    if (text.isEmpty()) {
        qCWarning(logDBusServer) << "Empty text provided for stream synthesis";
        return "";
    }
    
    qCInfo(logDBusServer) << "StartStreamSynthesis called for session:" << sessionId << "text:" << text.left(50) << "...";
    
    // Parse parameters
    QVariantMap varMapParams;
    if (!params.isEmpty()) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError) {
            varMapParams = doc.object().toVariantMap();
        } else {
            qCWarning(logDBusServer) << "Failed to parse stream synthesis parameters:" << error.errorString();
        }
    }
    
    QMutexLocker locker(&stateMutex);
    
    // Create provider and interface if not already created
    if (!currentTTSInterface) {
        auto modelCenter = getModelCenter();
        if (!modelCenter) {
            qCWarning(logDBusServer) << "ModelCenter not available for session:" << sessionId;
            return "";
        }
        
        // Convert QVariantMap to QVariantHash for ModelCenter
        QVariantHash varHashParams;
        for (auto it = varMapParams.begin(); it != varMapParams.end(); ++it) {
            varHashParams.insert(it.key(), it.value());
        }
        
        auto provider = modelCenter->createProviderForInterface(TextToSpeech, varHashParams, sessionId);
        if (!provider) {
            qCWarning(logDBusServer) << "Failed to create TextToSpeech provider for session:" << sessionId;
            return "";
        }
        
        currentTTSInterface = QSharedPointer<TextToSpeechInterface>(
            dynamic_cast<TextToSpeechInterface*>(provider->createInterface(TextToSpeech))
        );
        
        if (!currentTTSInterface) {
            qCWarning(logDBusServer) << "Failed to create TextToSpeech interface for session:" << sessionId;
            return "";
        }
        
        // Connect interface signals to handler slots
        connect(currentTTSInterface.data(), &TextToSpeechInterface::synthesisResult,
                this, &TextToSpeechSession::onInterfaceSynthesisResult);
        connect(currentTTSInterface.data(), &TextToSpeechInterface::synthesisError,
                this, &TextToSpeechSession::onInterfaceSynthesisError);
        connect(currentTTSInterface.data(), &TextToSpeechInterface::synthesisCompleted,
                this, &TextToSpeechSession::onInterfaceSynthesisCompleted);
    }
    
    // Start stream synthesis using the interface
    QVariantHash varHashParams;
    for (auto it = varMapParams.begin(); it != varMapParams.end(); ++it) {
        varHashParams[it.key()] = it.value();
    }
    QString streamSessionId = currentTTSInterface->startStreamSynthesis(text, varHashParams);
    if (streamSessionId.isEmpty()) {
        qCWarning(logDBusServer) << "Failed to start stream synthesis for session:" << sessionId;
        return "";
    }
    
    // Create stream session
    StreamSession session;
    session.streamSessionId = streamSessionId;
    session.isActive = true;
    
    streamSessions[streamSessionId] = session;
    synthesizing = true;
    
    qCInfo(logDBusServer) << "Started stream synthesis session:" << streamSessionId << "for session:" << sessionId;
    return streamSessionId;
}

QString TextToSpeechSession::endStreamSynthesis(const QString &streamSessionId)
{
    if (streamSessionId.isEmpty()) {
        qCWarning(logDBusServer) << "Empty stream session ID provided";
        return "";
    }
    
    qCInfo(logDBusServer) << "EndStreamSynthesis called for stream session:" << streamSessionId;
    
    QMutexLocker locker(&stateMutex);
    
    auto it = streamSessions.find(streamSessionId);
    if (it == streamSessions.end()) {
        qCWarning(logDBusServer) << "Stream session not found:" << streamSessionId;
        return "";
    }
    
    StreamSession &session = it.value();
    if (!session.isActive) {
        qCWarning(logDBusServer) << "Stream session is not active:" << streamSessionId;
        return "";
    }
    
    // End stream synthesis using the interface
    if (currentTTSInterface) {
        auto result = currentTTSInterface->endStreamSynthesis(streamSessionId);
        session.isActive = false;
        
        // Convert result to JSON string
        QJsonDocument doc(result);
        QString resultString = doc.toJson(QJsonDocument::Compact);
        
        qCInfo(logDBusServer) << "Ended stream synthesis session:" << streamSessionId;
        return resultString;
    } else {
        qCWarning(logDBusServer) << "TTS interface not available for stream session:" << streamSessionId;
        return "";
    }
}

void TextToSpeechSession::onInterfaceSynthesisResult(const QString &sessionId, const QByteArray &audioData)
{
    qCDebug(logDBusServer) << "Received synthesis result for stream session:" << sessionId << "size:" << audioData.size();
    emit SynthesisResult(sessionId, audioData);
}

void TextToSpeechSession::onInterfaceSynthesisError(const QString &sessionId, int errorCode, const QString &message)
{
    qCWarning(logDBusServer) << "Synthesis error for stream session:" << sessionId << "error:" << errorCode << message;
    emit SynthesisError(sessionId, errorCode, message);
}

void TextToSpeechSession::onInterfaceSynthesisCompleted(const QString &sessionId, const QByteArray &finalAudio)
{
    qCInfo(logDBusServer) << "Synthesis completed for stream session:" << sessionId << "total size:" << finalAudio.size();
    emit SynthesisCompleted(sessionId, finalAudio);
}

TaskManager::Task TextToSpeechSession::createStreamSynthesisTask(const QString &streamSessionId, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = AITypes::ServiceType::TextToSpeech;
    task.description = QString("Stream synthesis for session: %1").arg(sessionId);
    task.metadata["sessionId"] = sessionId;
    task.metadata["streamSessionId"] = streamSessionId;
    task.metadata["stream"] = true;
    
    // Extract provider and model information if available
    task.providerName = params.value("provider").toString();
    task.modelName = params.value("model").toString();
    
    // Task executor
    task.executor = [this, streamSessionId, params, taskId = task.taskId]() {
        try {
            auto* modelCenter = getModelCenter();
            if (!modelCenter) {
                throw std::runtime_error("ModelCenter not available");
            }
            
            // Convert QVariantMap to QVariantHash for ModelCenter
            QVariantHash varParams;
            for (auto it = params.begin(); it != params.end(); ++it) {
                varParams.insert(it.key(), it.value());
            }
            
            // Create provider and interface
            auto provider = modelCenter->createProviderForInterface(TextToSpeech, varParams, sessionId);
            if (!provider) {
                throw std::runtime_error("Failed to create TextToSpeech provider");
            }
            
            auto ttsInterface = QSharedPointer<TextToSpeechInterface>(
                dynamic_cast<TextToSpeechInterface*>(provider->createInterface(TextToSpeech))
            );
            
            if (!ttsInterface) {
                throw std::runtime_error("Failed to create TextToSpeech interface");
            }
            
            // For stream synthesis, we need to implement a different approach
            // For now, we'll use the file-based approach with temporary data
            // TODO: Implement proper stream synthesis
            
            // Create a temporary result
            QJsonObject result;
            result["success"] = true;
            result["error_code"] = 0;
            result["error_message"] = "";
            result["audio_data"] = "";
            result["audio_size"] = 0;
            
            return QVariant::fromValue(result);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(QString("Stream synthesis failed: %1").arg(e.what()).toStdString());
        }
    };
    
    return task;
}
