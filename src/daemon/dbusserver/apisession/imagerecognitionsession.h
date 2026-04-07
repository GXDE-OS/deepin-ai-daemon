// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMAGERECOGNITIONSESSION_H
#define IMAGERECOGNITIONSESSION_H

#include "sessionbase.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelcenter_define.h"
#include "modelcenter/interfaces/imagerecognitioninterface.h"

#include <QDBusMessage>
#include <QMap>
#include <QVariantMap>
#include <QMutex>

AIDAEMON_BEGIN_NAMESPACE

/**
 * @brief D-Bus session for image recognition services
 * 
 * This class provides D-Bus interface for image recognition functionality,
 * supporting various input methods (file path, image data, URL).
 */
class ImageRecognitionSession : public SessionBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.Session.ImageRecognition")
    
public:
    explicit ImageRecognitionSession(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    ~ImageRecognitionSession() override;
    
    // SessionBase implementation
    AITypes::ServiceType getServiceType() const override { return AITypes::ServiceType::ImageRecognition; }
    bool initialize() override;
    void cleanup() override;
    
    // Recognition state
    bool isRecognizing() const { return recognizing; }
    
public slots:
    // D-Bus interface methods - using lowercase as per project standard
    void terminate();
    QString recognizeImage(const QString &imagePath, const QString &prompt, const QString &params);
    QString recognizeImageData(const QByteArray &imageData, const QString &prompt, const QString &params);
    QString recognizeImageUrl(const QString &imageUrl, const QString &prompt, const QString &params);
    
    // Information methods
    QStringList getSupportedImageFormats();
    int getMaxImageSize();
    
signals:
    // Recognition result signals
    void recognitionResult(const QString &sessionId, const QString &result);
    void recognitionError(const QString &sessionId, int errorCode, const QString &message);
    void recognitionCompleted(const QString &sessionId, const QString &finalResult);
    
private slots:
    void onTaskCompleted(quint64 taskId, const QVariant& result) override;
    void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage) override;
    
    // Interface signal handlers
    void onInterfaceRecognitionResult(const QString &sessionId, const QString &result);
    void onInterfaceRecognitionError(const QString &sessionId, int errorCode, const QString &message);
    void onInterfaceRecognitionCompleted(const QString &sessionId, const QString &finalResult);
    
private:
    // Task creation helpers
    TaskManager::Task createImageRecognitionTask(const QList<VisionMessage> &messages, const QVariantMap &params);
    
    // Helper methods
    void handleImageRecognitionResult(quint64 taskId, bool success, const QVariant& result);
    void sendRecognitionResponse(const QDBusMessage& message, const QString& response);
    void sendRecognitionError(const QDBusMessage& message, int errorCode, const QString& errorMsg);
    
    // Utility methods
    QByteArray loadImageFile(const QString &imagePath);
    bool validateImageFormat(const QByteArray &imageData);
    QList<VisionMessage> createMessages(const QString &prompt, const QString &imagePath = QString(), 
                                       const QByteArray &imageData = QByteArray(), const QString &imageUrl = QString());
    
    // State management
    bool recognizing = false;
    QMap<quint64, QDBusMessage> pendingMessages;
    QSharedPointer<ImageRecognitionInterface> currentImageInterface;
    
    mutable QMutex stateMutex;
};

AIDAEMON_END_NAMESPACE

#endif // IMAGERECOGNITIONSESSION_H

