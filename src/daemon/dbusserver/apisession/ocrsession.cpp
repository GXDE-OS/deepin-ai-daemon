// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocrsession.h"
#include "sessionmanager/sessionmanager.h"
#include "common/aitypes.h"
#include "modelcenter/modelcenter.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImageReader>
#include <QBuffer>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QThread>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMutexLocker>

AIDAEMON_USE_NAMESPACE

OCRSession::OCRSession(const QString &sessionId, SessionManager *manager, QObject *parent)
    : SessionBase(sessionId, manager, parent)
{
    qInfo() << "Creating OCR session:" << sessionId;
}

OCRSession::~OCRSession()
{
    qInfo() << "Destroying OCR session:" << getSessionId();
    cleanup();
}

bool OCRSession::initialize()
{
    qInfo() << "Initializing OCR session:" << getSessionId();
    
    try {
        // Get ModelCenter to create OCR interface
        auto* modelCenter = getModelCenter();
        if (!modelCenter) {
            qWarning() << "ModelCenter not available for OCR session:" << getSessionId();
            return false;
        }
        
        // Create provider for OCR interface
        QVariantHash params; // Use default provider (DTK)
        auto provider = modelCenter->createProviderForInterface(OCR, params, getSessionId());
        if (!provider) {
            qWarning() << "Failed to create OCR provider for session:" << getSessionId();
            return false;
        }
        
        // Create OCR interface
        currentOCRInterface = QSharedPointer<OCRInterface>(
            dynamic_cast<OCRInterface*>(provider->createInterface(OCR))
        );
        
        if (!currentOCRInterface) {
            qWarning() << "Failed to create OCR interface for session:" << getSessionId();
            return false;
        }
        
        // Update supported capabilities from interface
        supportedLanguages = currentOCRInterface->getSupportedLanguages();
        supportedFormats = currentOCRInterface->getSupportedFormats();
        
        qInfo() << "OCR session initialized successfully";
        qInfo() << "Supported languages:" << supportedLanguages;
        qInfo() << "Supported formats:" << supportedFormats;
        
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "Exception during OCR session initialization:" << e.what();
        return false;
    }
}

void OCRSession::cleanup()
{
    qInfo() << "Cleaning up OCR session:" << getSessionId();
    
    QMutexLocker locker(&stateMutex);
    
    // Send error replies for pending messages
    for (auto it = pendingMessages.begin(); it != pendingMessages.end(); ++it) {
        sendRecognitionError(it.value(), ERROR_CODE_TERMINATION, "Session terminated");
    }
    pendingMessages.clear();
    
    // Cleanup interface
    if (currentOCRInterface) {
        currentOCRInterface->terminate();
        currentOCRInterface.reset();
    }
    
    recognizing = false;
}

void OCRSession::terminate()
{
    qInfo() << "Terminating OCR session:" << getSessionId();
    cleanup();
}

QString OCRSession::recognizeFile(const QString &imageFile, const QString &params)
{
    qInfo() << "OCR recognizeFile called:" << imageFile << "params:" << params;
    
    if (!currentOCRInterface) {
        return createErrorJson(ERROR_CODE_INTERFACE_NOT_AVAILABLE, "OCR interface not available");
    }
    
    if (!QFileInfo::exists(imageFile)) {
        return createErrorJson(ERROR_CODE_INVALID_INPUT, "Image file does not exist");
    }
    
    // Parse parameters
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8(), &error);
    QVariantMap paramMap;
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        paramMap = doc.object().toVariantMap();
    }
    
    // Create and execute task
    auto task = createFileRecognitionTask(imageFile, paramMap);
    submitTask(task);
    quint64 taskId = task.taskId;
    
    // Store pending message for delayed reply
    QMutexLocker locker(&stateMutex);
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[taskId] = msg;
    recognizing = true;
    
    qInfo() << "OCR file recognition task submitted:" << taskId << "for session:" << getSessionId() << "file:" << imageFile;
    
    return ""; // Delayed reply
}

QString OCRSession::recognizeImage(const QByteArray &imageData, const QString &params)
{
    qInfo() << "OCR recognizeImage called, data size:" << imageData.size() << "params:" << params;
    
    if (imageData.isEmpty()) {
        return createErrorJson(ERROR_CODE_INTERFACE_NOT_AVAILABLE, "Empty image data");
    }
    
    if (!currentOCRInterface) {
        return createErrorJson(ERROR_CODE_INTERFACE_NOT_AVAILABLE, "OCR interface not available");
    }
    
    // Parse parameters
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8(), &error);
    QVariantMap paramMap;
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        paramMap = doc.object().toVariantMap();
    }
    
    // Create and execute task
    auto task = createImageRecognitionTask(imageData, paramMap);
    submitTask(task);
    quint64 taskId = task.taskId;
    
    // Store pending message for delayed reply
    QMutexLocker locker(&stateMutex);
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[taskId] = msg;
    recognizing = true;
    
    qInfo() << "OCR image recognition task submitted:" << taskId << "for session:" << getSessionId() << "data size:" << imageData.size();
    
    return ""; // Delayed reply
}

QString OCRSession::recognizeRegion(const QString &imageFile, const QString &region, const QString &params)
{
    qInfo() << "OCR recognizeRegion called:" << imageFile << "region:" << region << "params:" << params;
    
    if (!currentOCRInterface) {
        return createErrorJson(ERROR_CODE_INTERFACE_NOT_AVAILABLE, "OCR interface not available");
    }
    
    // Parse region
    QRect regionRect = parseRegion(region);
    if (!regionRect.isValid()) {
        return createErrorJson(ERROR_CODE_INVALID_INPUT, "Invalid region format");
    }
    
    if (!QFileInfo::exists(imageFile)) {
        return createErrorJson(ERROR_CODE_INVALID_INPUT, "Image file does not exist");
    }
    
    // Parse parameters
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8(), &error);
    QVariantMap paramMap;
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        paramMap = doc.object().toVariantMap();
    }
    
    // Create and execute task
    auto task = createRegionRecognitionTask(imageFile, regionRect, paramMap);
    submitTask(task);
    quint64 taskId = task.taskId;
    
    // Store pending message for delayed reply
    QMutexLocker locker(&stateMutex);
    QDBusMessage msg = message();
    msg.setDelayedReply(true);
    pendingMessages[taskId] = msg;
    recognizing = true;
    
    qInfo() << "OCR region recognition task submitted:" << taskId << "for session:" << getSessionId() 
            << "file:" << imageFile << "region:" << regionRect;
    
    return ""; // Delayed reply
}

bool OCRSession::cancel(const QString &taskId)
{
    Q_UNUSED(taskId)
    // Note: Individual task cancellation with delayed replies is complex
    // For now, we rely on the TaskManager's general cancellation mechanism
    qInfo() << "OCR cancel called for session:" << getSessionId();
    return true;
}

QStringList OCRSession::getSupportedLanguages()
{
    if (currentOCRInterface) {
        return currentOCRInterface->getSupportedLanguages();
    }
    return supportedLanguages;
}

QStringList OCRSession::getSupportedFormats()
{
    if (currentOCRInterface) {
        return currentOCRInterface->getSupportedFormats();
    }
    return supportedFormats;
}

QString OCRSession::getCapabilities()
{
    if (currentOCRInterface) {
        QJsonObject capabilities = currentOCRInterface->getCapabilities();
        return QJsonDocument(capabilities).toJson(QJsonDocument::Compact);
    }
    
    return {};
}

void OCRSession::onTaskCompleted(quint64 taskId, const QVariant& result)
{
    handleRecognitionResult(taskId, true, result);
}

void OCRSession::onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage)
{
    handleRecognitionResult(taskId, false, QVariant::fromValue(QPair<int, QString>(errorCode, errorMessage)));
}

TaskManager::Task OCRSession::createFileRecognitionTask(const QString &imageFile, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = TaskManager::TaskType::OCR;
    task.description = QString("OCR file recognition: %1").arg(imageFile);
    
    task.executor = [this, imageFile, params]() -> QVariant {
        return performFileRecognition(imageFile, params);
    };
    
    return task;
}

TaskManager::Task OCRSession::createImageRecognitionTask(const QByteArray &imageData, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = TaskManager::TaskType::OCR;
    task.description = QString("OCR image recognition, data size: %1").arg(imageData.size());
    
    task.executor = [this, imageData, params]() -> QVariant {
        return performImageRecognition(imageData, params);
    };
    
    return task;
}

TaskManager::Task OCRSession::createRegionRecognitionTask(const QString &imageFile, const QRect &region, const QVariantMap &params)
{
    TaskManager::Task task;
    task.taskType = TaskManager::TaskType::OCR;
    task.description = QString("OCR region recognition: %1, region: %2").arg(imageFile).arg(QString("%1,%2,%3,%4").arg(region.x()).arg(region.y()).arg(region.width()).arg(region.height()));
    
    task.executor = [this, imageFile, region, params]() -> QVariant {
        return performRegionRecognition(imageFile, region, params);
    };
    
    return task;
}

void OCRSession::handleRecognitionResult(quint64 taskId, bool success, const QVariant& result)
{
    QMutexLocker locker(&stateMutex);
    
    // Find pending message
    auto msgIt = pendingMessages.find(taskId);
    if (msgIt == pendingMessages.end()) {
        qWarning() << "No pending message found for task:" << taskId;
        return;
    }
    
    QDBusMessage msg = msgIt.value();
    
    // Clean up
    pendingMessages.erase(msgIt);
    recognizing = pendingMessages.isEmpty();
    
    locker.unlock();
    
    // Send response
    if (success) {
        QString resultText;
        if (result.canConvert<QJsonObject>()) {
            QJsonObject jsonResult = result.value<QJsonObject>();
            resultText = QJsonDocument(jsonResult).toJson(QJsonDocument::Compact);
        } else {
            resultText = result.toString();
        }
        sendRecognitionResponse(msg, resultText);
    } else {
        auto errorPair = result.value<QPair<int, QString>>();
        sendRecognitionError(msg, errorPair.first, errorPair.second);
    }
}

void OCRSession::sendRecognitionResponse(const QDBusMessage& message, const QString& response)
{
    if (message.type() == QDBusMessage::InvalidMessage) {
        return;
    }
    
    QDBusMessage reply = message.createReply(QVariantList() << response);
    QDBusConnection::sessionBus().send(reply);
}

void OCRSession::sendRecognitionError(const QDBusMessage& message, int errorCode, const QString& errorMsg)
{
    if (message.type() == QDBusMessage::InvalidMessage) {
        return;
    }
    
    QDBusMessage error = message.createErrorReply(QString("org.deepin.ai.daemon.OCRError.%1").arg(errorCode), errorMsg);
    QDBusConnection::sessionBus().send(error);
}

QString OCRSession::performFileRecognition(const QString &imageFile, const QVariantMap &params)
{
    if (!currentOCRInterface) {
        throw std::runtime_error("OCR interface not available");
    }
    
    try {
        QVariantHash paramsHash;
        for (auto it = params.begin(); it != params.end(); ++it) {
            paramsHash.insert(it.key(), it.value());
        }
        
        QJsonObject result = currentOCRInterface->recognizeFile(imageFile, paramsHash);
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
        
    } catch (const std::exception& e) {
        throw std::runtime_error(QString("File recognition failed: %1").arg(e.what()).toStdString());
    }
}

QString OCRSession::performImageRecognition(const QByteArray &imageData, const QVariantMap &params)
{
    if (!currentOCRInterface) {
        throw std::runtime_error("OCR interface not available");
    }
    
    try {
        QVariantHash paramsHash;
        for (auto it = params.begin(); it != params.end(); ++it) {
            paramsHash.insert(it.key(), it.value());
        }
        
        QJsonObject result = currentOCRInterface->recognizeImage(imageData, paramsHash);
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
        
    } catch (const std::exception& e) {
        throw std::runtime_error(QString("Image recognition failed: %1").arg(e.what()).toStdString());
    }
}

QString OCRSession::performRegionRecognition(const QString &imageFile, const QRect &region, const QVariantMap &params)
{
    if (!currentOCRInterface) {
        throw std::runtime_error("OCR interface not available");
    }
    
    try {
        QVariantHash paramsHash;
        for (auto it = params.begin(); it != params.end(); ++it) {
            paramsHash.insert(it.key(), it.value());
        }
        
        QJsonObject result = currentOCRInterface->recognizeRegion(imageFile, region, paramsHash);
        return QJsonDocument(result).toJson(QJsonDocument::Compact);
        
    } catch (const std::exception& e) {
        throw std::runtime_error(QString("Region recognition failed: %1").arg(e.what()).toStdString());
    }
}

QRect OCRSession::parseRegion(const QString &regionString)
{
    if (regionString.isEmpty()) {
        return QRect();
    }
    
    // Format 1: "x,y,width,height"
    if (regionString.contains(',') && !regionString.contains(';')) {
        QStringList parts = regionString.split(',');
        if (parts.size() == COORDINATE_PARTS_COUNT) {
            bool ok;
            int x = parts[0].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            int y = parts[1].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            int width = parts[2].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            int height = parts[3].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            return QRect(x, y, width, height);
        }
    }
    
    // Format 2: "x1,y1;x2,y2"
    if (regionString.contains(';')) {
        QStringList points = regionString.split(';');
        if (points.size() == TWO_POINTS_COUNT) {
            QStringList point1 = points[0].split(',');
            QStringList point2 = points[1].split(',');
            
            if (point1.size() != POINT_COORDINATE_COUNT || point2.size() != POINT_COORDINATE_COUNT) {
                return QRect();
            }
            
            bool ok;
            int x1 = point1[0].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            int y1 = point1[1].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            int x2 = point2[0].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            int y2 = point2[1].trimmed().toInt(&ok);
            if (!ok) return QRect();
            
            // Convert two points to rect (top-left and bottom-right)
            int left = qMin(x1, x2);
            int top = qMin(y1, y2);
            int width = qAbs(x2 - x1);
            int height = qAbs(y2 - y1);
            
            return QRect(left, top, width, height);
        }
    }
    
    // Invalid format
    return QRect();
}

QString OCRSession::createErrorJson(int errorCode, const QString &errorMessage)
{
    return QString(R"({"error":true,"error_code":%1, "%2"})")
           .arg(errorCode)
           .arg(errorMessage);
}
