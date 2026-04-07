// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMAGERECOGNITIONINTERFACE_H
#define IMAGERECOGNITIONINTERFACE_H

#include "abstractinterface.h"
#include <QObject>
#include <QJsonObject>
#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

/**
 * @brief Structure for vision message containing text and image data
 */
struct VisionMessage {
    QString text;           // Text prompt content
    QString imageUrl;       // Image URL for remote images
    QByteArray imageData;   // Image data (base64 encoded for local images)
    QString imageType;      // Image type: "url" or "base64"
    QString mimeType;
};

/**
 * @brief Abstract interface for image recognition services
 * 
 * This interface provides methods for recognizing and analyzing images
 * using AI models. It supports both synchronous and asynchronous operations.
 */
class ImageRecognitionInterface : public QObject, public AbstractInterface
{
    Q_OBJECT
    
public:
    explicit ImageRecognitionInterface(const QString &model, QObject *parent = nullptr);
    virtual ~ImageRecognitionInterface() override;
    
    // Basic interface methods
    virtual QString model() const;
    virtual void setModel(const QString &model);
    InterfaceType type() override;
    
    // Synchronous image recognition
    virtual QJsonObject recognizeImage(const QList<VisionMessage> &messages, 
                                      const QVariantHash &params = {}) = 0;
    
    // Asynchronous image recognition
    virtual QString startImageRecognition(const QList<VisionMessage> &messages,
                                         const QVariantHash &params = {}) = 0;
    
    // Control methods
    virtual void terminate() = 0;
    
    // Information methods
    virtual QStringList getSupportedImageFormats() const = 0;
    virtual int getMaxImageSize() const = 0;
    
signals:
    // Asynchronous recognition results
    void recognitionResult(const QString &sessionId, const QString &result);
    void recognitionError(const QString &sessionId, int errorCode, const QString &message);
    void recognitionCompleted(const QString &sessionId, const QString &finalResult);
    
protected:
    QString modelName;
};

AIDAEMON_END_NAMESPACE

#endif // IMAGERECOGNITIONINTERFACE_H

