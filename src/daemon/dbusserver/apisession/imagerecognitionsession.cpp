// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagerecognitionsession.h"
#include "sessionmanager/sessionmanager.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelutils.h"
#include "modelcenter/modelcenter.h"
#include "modelcenter/interfaces/imagerecognitioninterface.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QThread>
#include <QUuid>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

ImageRecognitionSession::ImageRecognitionSession(const QString &sessionId, SessionManager *manager, QObject *parent)
    : SessionBase(sessionId, manager, parent)
{
    qCDebug(logDBusServer) << "ImageRecognitionSession created:" << sessionId;
}

ImageRecognitionSession::~ImageRecognitionSession()
{
    qCDebug(logDBusServer) << "ImageRecognitionSession destroyed:" << sessionId;
}

bool ImageRecognitionSession::initialize()
{    
    qCInfo(logDBusServer) << "Initializing ImageRecognition session:" << sessionId;
    
    try {
        // Get ModelCenter to create ImageRecognition interface
        auto* modelCenter = getModelCenter();
        if (!modelCenter) {
            qCWarning(logDBusServer) << "ModelCenter not available for ImageRecognition session:" << sessionId;
            return false;
        }
        
        // Create provider for ImageRecognition interface
        QVariantHash params; // Use default provider from configuration
        auto provider = modelCenter->createProviderForInterface(ImageRecognition, params, sessionId);
        if (!provider) {
            qCWarning(logDBusServer) << "Failed to create ImageRecognition provider for session:" << sessionId;
            return false;
        }
        
        // Create ImageRecognition interface
        currentImageInterface = QSharedPointer<ImageRecognitionInterface>(
            dynamic_cast<ImageRecognitionInterface*>(provider->createInterface(ImageRecognition))
        );
        
        if (!currentImageInterface) {
            qCWarning(logDBusServer) << "Failed to create ImageRecognition interface for session:" << sessionId;
            return false;
        }
        
        // Connect interface signals to session slots
        connect(currentImageInterface.data(), &ImageRecognitionInterface::recognitionResult,
                this, &ImageRecognitionSession::onInterfaceRecognitionResult);
        connect(currentImageInterface.data(), &ImageRecognitionInterface::recognitionError,
                this, &ImageRecognitionSession::onInterfaceRecognitionError);
        connect(currentImageInterface.data(), &ImageRecognitionInterface::recognitionCompleted,
                this, &ImageRecognitionSession::onInterfaceRecognitionCompleted);
        
        qCInfo(logDBusServer) << "ImageRecognitionSession initialized successfully:" << sessionId;
        return true;
        
    } catch (const std::exception& e) {
        qCCritical(logDBusServer) << "Exception during ImageRecognitionSession initialization:" << e.what() << "session:" << sessionId;
        return false;
    }
}

void ImageRecognitionSession::cleanup()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    recognizing = false;
    pendingMessages.clear();
    
    // Clean up image interface
    if (currentImageInterface) {
        currentImageInterface->terminate();
        currentImageInterface.reset();
    }
    
    qCInfo(logDBusServer) << "ImageRecognitionSession cleaned up:" << sessionId;
}

void ImageRecognitionSession::terminate()
{
    QMutexLocker locker(&stateMutex);
    
    // Cancel all active tasks
    cleanupTasks();
    
    // Clear state
    recognizing = false;
    pendingMessages.clear();
    
    qCInfo(logDBusServer) << "ImageRecognitionSession terminated:" << sessionId;
}

QString ImageRecognitionSession::recognizeImage(const QString &imagePath, const QString &prompt, const QString &params)
{
    if (imagePath.isEmpty()) {
        qCWarning(logDBusServer) << "Empty image path received for session:" << sessionId;
        return R"({"error":true,"error_code":1,"error_message":"Empty image path"})";
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (recognizing) {
        qCWarning(logDBusServer) << "Recognition request rejected: Recognition in progress for session:" << sessionId;
        return R"({"error":true,"error_code":1,"error_message":"Recognition in progress"})";
    }
    
    // Load and validate image file
    QByteArray imageData = loadImageFile(imagePath);
    if (imageData.isEmpty()) {
        qCWarning(logDBusServer) << "Failed to load image file:" << imagePath;
        return R"({"error":true,"error_code":2,"error_message":"Failed to load image file"})";
    }
    
    if (!validateImageFormat(imageData)) {
        qCWarning(logDBusServer) << "Unsupported image format:" << imagePath;
        return R"({"error":true,"error_code":3,"error_message":"Unsupported image format"})";
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create messages
    QList<VisionMessage> messages = createMessages(prompt, imagePath, imageData);
    
    // Create recognition task
    auto recognitionTask = createImageRecognitionTask(messages, varParams);
    
    // Store D-Bus message for delayed reply
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[recognitionTask.taskId] = msg;
    
    recognizing = true;
    
    // Submit task to TaskManager
    submitTask(recognitionTask);
    
    qCInfo(logDBusServer) << "Image recognition task submitted:" << recognitionTask.taskId 
                          << "for session:" << sessionId
                          << "image:" << imagePath;
    
    return ""; // Delayed reply
}

QString ImageRecognitionSession::recognizeImageData(const QByteArray &imageData, const QString &prompt, const QString &params)
{
    if (imageData.isEmpty()) {
        qCWarning(logDBusServer) << "Empty image data received for session:" << sessionId;
        return R"({"error":true,"error_code":1,"error_message":"Empty image data"})";
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (recognizing) {
        qCWarning(logDBusServer) << "Recognition request rejected: Recognition in progress for session:" << sessionId;
        return R"({"error":true,"error_code":1,"error_message":"Recognition in progress"})";
    }
    
    if (!validateImageFormat(imageData)) {
        qCWarning(logDBusServer) << "Unsupported image format in image data";
        return R"({"error":true,"error_code":3,"error_message":"Unsupported image format"})";
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create messages
    QList<VisionMessage> messages = createMessages(prompt, QString(), imageData);
    
    // Create recognition task
    auto recognitionTask = createImageRecognitionTask(messages, varParams);
    
    // Store D-Bus message for delayed reply
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[recognitionTask.taskId] = msg;
    
    recognizing = true;
    
    // Submit task to TaskManager
    submitTask(recognitionTask);
    
    qCInfo(logDBusServer) << "Image data recognition task submitted:" << recognitionTask.taskId 
                          << "for session:" << sessionId
                          << "data size:" << imageData.size();
    
    return ""; // Delayed reply
}

QString ImageRecognitionSession::recognizeImageUrl(const QString &imageUrl, const QString &prompt, const QString &params)
{
    if (imageUrl.isEmpty()) {
        qCWarning(logDBusServer) << "Empty image URL received for session:" << sessionId;
        return R"({"error":true,"error_code":1,"error_message":"Empty image URL"})";
    }
    
    QMutexLocker locker(&stateMutex);
    
    if (recognizing) {
        qCWarning(logDBusServer) << "Recognition request rejected: Recognition in progress for session:" << sessionId;
        return R"({"error":true,"error_code":1,"error_message":"Recognition in progress"})";
    }
    
    // Parse parameters
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varHash = doc.object().toVariantHash();
    QVariantMap varParams;
    for (auto it = varHash.begin(); it != varHash.end(); ++it) {
        varParams.insert(it.key(), it.value());
    }
    
    // Create messages
    QList<VisionMessage> messages = createMessages(prompt, QString(), QByteArray(), imageUrl);
    
    // Create recognition task
    auto recognitionTask = createImageRecognitionTask(messages, varParams);
    
    // Store D-Bus message for delayed reply
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[recognitionTask.taskId] = msg;
    
    recognizing = true;
    
    // Submit task to TaskManager
    submitTask(recognitionTask);
    
    qCInfo(logDBusServer) << "Image URL recognition task submitted:" << recognitionTask.taskId 
                          << "for session:" << sessionId
                          << "URL:" << imageUrl;
    
    return ""; // Delayed reply
}

QStringList ImageRecognitionSession::getSupportedImageFormats()
{
    // Get supported formats from the current interface
    if (currentImageInterface) {
        return currentImageInterface->getSupportedImageFormats();
    }
    
    qCWarning(logDBusServer) << "ImageRecognition interface not initialized for session:" << sessionId;
    
    return {};
}

int ImageRecognitionSession::getMaxImageSize()
{
    // Get max image size from the current interface
    if (currentImageInterface) {
        return currentImageInterface->getMaxImageSize();
    }
    
    qCWarning(logDBusServer) << "ImageRecognition interface not initialized for session:" << sessionId;
    
    // Fallback to default max image size (10MB)
    return 10 * 1024 * 1024;
}

void ImageRecognitionSession::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    handleImageRecognitionResult(taskId, true, result);
}

void ImageRecognitionSession::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    Q_UNUSED(errorCode)
    handleImageRecognitionResult(taskId, false, errorMessage);
}

void ImageRecognitionSession::onInterfaceRecognitionResult(const QString &sessionId, const QString &result)
{
    qCDebug(logDBusServer) << "Interface recognition result for session:" << sessionId 
                           << "result:" << result << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit recognitionResult(sessionId, result);
}

void ImageRecognitionSession::onInterfaceRecognitionError(const QString &sessionId, int errorCode, const QString &message)
{
    qCWarning(logDBusServer) << "Interface recognition error for session:" << sessionId 
                             << "error:" << errorCode << message << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit recognitionError(sessionId, errorCode, message);
}

void ImageRecognitionSession::onInterfaceRecognitionCompleted(const QString &sessionId, const QString &finalResult)
{
    qCInfo(logDBusServer) << "Interface recognition completed for session:" << sessionId 
                          << "result:" << finalResult << "session:" << this->sessionId;
    
    // Forward to D-Bus signal
    emit recognitionCompleted(sessionId, finalResult);
}

TaskManager::Task ImageRecognitionSession::createImageRecognitionTask(const QList<VisionMessage> &messages, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = AITypes::ServiceType::ImageRecognition;
    task.description = QString("Image recognition for session: %1").arg(sessionId);
    task.metadata["sessionId"] = sessionId;
    task.metadata["messageCount"] = messages.size();
    
    // Extract provider and model information if available
    task.providerName = params.value("provider").toString();
    task.modelName = params.value("model").toString();
    
    // Task executor - avoid accessing this pointer in async context
    task.executor = [messages, params, taskId = task.taskId, sessionId = this->sessionId, sessionManager = this->sessionManager]() {
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
            auto provider = modelCenter->createProviderForInterface(ImageRecognition, varParams, sessionId);
            if (!provider) {
                throw std::runtime_error("Failed to create ImageRecognition provider");
            }
            
            auto imageInterface = QSharedPointer<ImageRecognitionInterface>(
                dynamic_cast<ImageRecognitionInterface*>(provider->createInterface(ImageRecognition))
            );
            
            if (!imageInterface) {
                throw std::runtime_error("Failed to create ImageRecognition interface");
            }
            
            // Perform recognition
            auto result = imageInterface->recognizeImage(messages, varParams);
            
            // Return result
            return QVariant::fromValue(result);
            
        } catch (const std::exception& e) {
            throw std::runtime_error(QString("Image recognition failed: %1").arg(e.what()).toStdString());
        }
    };
    
    return task;
}

void ImageRecognitionSession::handleImageRecognitionResult(quint64 taskId, bool success, const QVariant& result)
{
    QMutexLocker locker(&stateMutex);
    
    // Find pending message
    auto msgIt = pendingMessages.find(taskId);
    if (msgIt == pendingMessages.end()) {
        qCWarning(logDBusServer) << "No pending message found for task:" << taskId;
        return;
    }
    
    QDBusMessage msg = msgIt.value();
    
    // Clean up
    pendingMessages.erase(msgIt);
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
}

void ImageRecognitionSession::sendRecognitionResponse(const QDBusMessage& message, const QString& response)
{
    QDBusMessage reply = message.createReply(response);
    QDBusConnection::sessionBus().send(reply);
}

void ImageRecognitionSession::sendRecognitionError(const QDBusMessage& message, int errorCode, const QString& errorMsg)
{
    QJsonObject error;
    error["error"] = true;
    error["error_code"] = errorCode;
    error["error_message"] = errorMsg;
    error["content"] = "";
    
    QString response = QJsonDocument(error).toJson(QJsonDocument::Compact);
    sendRecognitionResponse(message, response);
}

QByteArray ImageRecognitionSession::loadImageFile(const QString &imagePath)
{
    QFile file(imagePath);
    if (!file.exists()) {
        qCWarning(logDBusServer) << "Image file does not exist:" << imagePath;
        return QByteArray();
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logDBusServer) << "Failed to open image file:" << imagePath << file.errorString();
        return QByteArray();
    }
    
    QByteArray data = file.readAll();
    if (data.isEmpty()) {
        qCWarning(logDBusServer) << "Image file is empty:" << imagePath;
        return QByteArray();
    }
    
    // Check file size
    if (data.size() > getMaxImageSize()) {
        qCWarning(logDBusServer) << "Image file too large:" << imagePath << "size:" << data.size();
        return QByteArray();
    }
    
    qCDebug(logDBusServer) << "Loaded image file:" << imagePath << "size:" << data.size();
    return data;
}

bool ImageRecognitionSession::validateImageFormat(const QByteArray &imageData)
{
    if (imageData.isEmpty()) {
        return false;
    }
    
    // Use QMimeDatabase to detect image format
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForData(imageData);
    
    if (!mimeType.name().startsWith("image/")) {
        qCWarning(logDBusServer) << "Not an image file, detected mime type:" << mimeType.name();
        return false;
    }
    
    // Check if format is supported
    QStringList supportedFormats = getSupportedImageFormats();
    QString format = mimeType.name().section('/', 1); // Get format part after "image/"
    
    // Handle special cases
    if (format == "jpeg") {
        format = "jpg";
    }
    
    bool supported = supportedFormats.contains(format, Qt::CaseInsensitive);
    if (!supported) {
        qCWarning(logDBusServer) << "Unsupported image format:" << format;
    }
    
    return supported;
}

QString detectImageFormat(const QByteArray &imageData)
{
    // Use QMimeDatabase to detect image format
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForData(imageData);

    if (!mimeType.name().startsWith("image/")) {
        // If we can't detect the image format, default to jpeg
        qCWarning(logDBusServer) << "Failed to detect image format, defaulting to jpeg. Mime type:" << mimeType.name();
        return "jpeg";
    }

    // Extract format from mime type (e.g., "image/jpeg" -> "jpeg")
    QString format = mimeType.name().section('/', 1);

    // Handle special cases
    if (format == "jpg") {
        format = "jpeg";  // API expects "jpeg" not "jpg"
    }

    qCDebug(logDBusServer) << "Detected image format:" << format;
    return format;
}

QList<VisionMessage> ImageRecognitionSession::createMessages(const QString &prompt, const QString &imagePath, 
                                                            const QByteArray &imageData, const QString &imageUrl)
{
    QList<VisionMessage> messages;
    VisionMessage message;
    
    // Set text prompt
    if (!prompt.isEmpty()) {
        message.text = prompt;
    } else {
        message.text = "What is in this image?"; // Default prompt
    }
    
    // Set image data
    if (!imageUrl.isEmpty()) {
        message.imageUrl = imageUrl;
        message.imageType = "url";
    } else if (!imageData.isEmpty()) {
        message.imageData = imageData.toBase64();
        message.imageType = "base64";
        message.mimeType = detectImageFormat(imageData);
    }
    
    messages.append(message);
    return messages;
}

