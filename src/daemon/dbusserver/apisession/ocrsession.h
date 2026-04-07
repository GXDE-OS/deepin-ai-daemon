// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OCRSESSION_H
#define OCRSESSION_H

#include "sessionbase.h"
#include "taskmanager/taskmanager.h"
#include "modelcenter/modelcenter_define.h"
#include "modelcenter/interfaces/ocrinterface.h"

#include <QDBusMessage>
#include <QMap>
#include <QVariantMap>
#include <QMutex>
#include <QImage>
#include <QRect>

AIDAEMON_BEGIN_NAMESPACE

class OCRSession : public SessionBase
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.Session.OCR")
    
    // Constants for OCR session operations
    static constexpr int ERROR_CODE_INTERFACE_NOT_AVAILABLE = 1;
    static constexpr int ERROR_CODE_INVALID_INPUT = 2;
    static constexpr int ERROR_CODE_TERMINATION = 999;
    
    static constexpr int COORDINATE_PARTS_COUNT = 4;
    static constexpr int POINT_COORDINATE_COUNT = 2;
    static constexpr int TWO_POINTS_COUNT = 2;
    
public:
    explicit OCRSession(const QString &sessionId, SessionManager *manager, QObject *parent = nullptr);
    ~OCRSession() override;
    
    // SessionBase implementation
    AITypes::ServiceType getServiceType() const override { return AITypes::ServiceType::OCR; }
    bool initialize() override;
    void cleanup() override;
    
    // Recognition state
    bool isRecognizing() const { return recognizing; }
    
public slots:
    // D-Bus interface methods - business logic resides here
    void terminate();
    
    // Recognition methods
    QString recognizeFile(const QString &imageFile, const QString &params);
    QString recognizeImage(const QByteArray &imageData, const QString &params);
    QString recognizeRegion(const QString &imageFile, const QString &region, const QString &params);

    // Control methods
    bool cancel(const QString &taskId);
    
    // Information methods
    QStringList getSupportedLanguages();
    QStringList getSupportedFormats();
    QString getCapabilities();
    
private slots:
    void onTaskCompleted(quint64 taskId, const QVariant& result) override;
    void onTaskFailed(quint64 taskId, int errorCode, const QString& errorMessage) override;
    
private:
    // Task creation helpers
    TaskManager::Task createFileRecognitionTask(const QString &imageFile, const QVariantMap &params);
    TaskManager::Task createImageRecognitionTask(const QByteArray &imageData, const QVariantMap &params);
    TaskManager::Task createRegionRecognitionTask(const QString &imageFile, const QRect &region, const QVariantMap &params);
    
    // Helper methods
    void handleRecognitionResult(quint64 taskId, bool success, const QVariant& result);
    void sendRecognitionResponse(const QDBusMessage& message, const QString& response);
    void sendRecognitionError(const QDBusMessage& message, int errorCode, const QString& errorMsg);
    
    // OCR processing methods
    QString performFileRecognition(const QString &imageFile, const QVariantMap &params);
    QString performImageRecognition(const QByteArray &imageData, const QVariantMap &params);
    QString performRegionRecognition(const QString &imageFile, const QRect &region, const QVariantMap &params);


    // Helper methods  
    QRect parseRegion(const QString &regionString);
    QString createErrorJson(int errorCode, const QString &errorMessage);
    
    // State management
    bool recognizing = false;
    QMap<quint64, QDBusMessage> pendingMessages;  // For delayed replies
    
    // OCR interface
    QSharedPointer<OCRInterface> currentOCRInterface;
    
    mutable QMutex stateMutex;
    
    // Configuration
    QString defaultLanguage = "zh-Hans_en";
    QStringList supportedFormats;
    QStringList supportedLanguages;
};

AIDAEMON_END_NAMESPACE

#endif // OCRSESSION_H
