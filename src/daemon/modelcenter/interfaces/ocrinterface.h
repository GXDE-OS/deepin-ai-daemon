// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OCRINTERFACE_H
#define OCRINTERFACE_H

#include "abstractinterface.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>
#include <QRect>

AIDAEMON_BEGIN_NAMESPACE

class OCRInterface : public QObject, public AbstractInterface
{
    Q_OBJECT
public:
    explicit OCRInterface(const QString &model, QObject *parent = nullptr);
    ~OCRInterface();
    
    virtual QString model() const;
    virtual void setModel(const QString &model);
    InterfaceType type() override;
    
    // File-based OCR recognition
    virtual QJsonObject recognizeFile(const QString &imageFile, 
                                     const QVariantHash &params = {}) = 0;
    
    // Image data-based OCR recognition
    virtual QJsonObject recognizeImage(const QByteArray &imageData,
                                      const QVariantHash &params = {}) = 0;
    
    // Region-based OCR recognition
    virtual QJsonObject recognizeRegion(const QString &imageFile,
                                       const QRect &region,
                                       const QVariantHash &params = {}) = 0;
    
    // Information methods
    virtual QStringList getSupportedLanguages() const = 0;
    virtual QStringList getSupportedFormats() const = 0;
    virtual QJsonObject getCapabilities() const = 0;
    
    // Control methods
    virtual void terminate() = 0;
    
signals:
    // OCR recognition results (for async operations if needed)
    void recognitionCompleted(const QString &sessionId, const QJsonObject &result);
    void recognitionError(const QString &sessionId, int errorCode, const QString &message);
    
protected:
    QString modelName;
};

AIDAEMON_END_NAMESPACE

#endif // OCRINTERFACE_H
