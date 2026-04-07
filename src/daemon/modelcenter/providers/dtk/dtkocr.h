// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DTKOCR_H
#define DTKOCR_H

#include "modelcenter/interfaces/ocrinterface.h"

#include <QSharedPointer>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>

// Forward declarations for DTK OCR
namespace Dtk {
namespace Ocr {
class DOcr;
}
}

AIDAEMON_BEGIN_NAMESPACE

class DTKOCRInterface : public OCRInterface
{
    Q_OBJECT
    
    // Constants for OCR operations
    static constexpr int ERROR_CODE_NOT_INITIALIZED = 1;
    static constexpr int ERROR_CODE_INVALID_INPUT = 2;
    static constexpr int ERROR_CODE_RECOGNITION_FAILED = 3;
    
    static constexpr int DEFAULT_HARDWARE_PRIORITY = 0;
    static constexpr int GPU_HARDWARE_THRESHOLD = 200;
    static constexpr double DEFAULT_CONFIDENCE_SCORE = 1.0;
    
    static constexpr int MAX_IMAGE_SIZE_BYTES = 4 * 1024 * 1024; // 4MB limit
    static constexpr int BYTES_PER_KB = 1024;
    static constexpr int KB_PER_MB = 1024;
    
public:
    explicit DTKOCRInterface(const QString &model, QObject *parent = nullptr);
    ~DTKOCRInterface() override;
    
    // Initialize the OCR engine
    bool initialize();
    void cleanup();
    
    // OCRInterface implementation
    QJsonObject recognizeFile(const QString &imageFile, 
                             const QVariantHash &params = {}) override;
    
    QJsonObject recognizeImage(const QByteArray &imageData,
                              const QVariantHash &params = {}) override;
    
    QJsonObject recognizeRegion(const QString &imageFile,
                               const QRect &region,
                               const QVariantHash &params = {}) override;
    
    // Information methods
    QStringList getSupportedLanguages() const override;
    QStringList getSupportedFormats() const override;
    QJsonObject getCapabilities() const override;
    
    // Control methods
    void terminate() override;
    
private:
    // Internal helper methods
    bool setupOCREngine();
    QString extractTextFromImage(const QString &imageFile, const QRect &region = QRect(), const QVariantHash &params = {});
    QString extractTextFromImageData(const QByteArray &imageData, const QVariantHash &params = {});
    QJsonObject formatOCRResult(const QString &text, const QVariantHash &params = {});
    QJsonObject createErrorResult(int errorCode, const QString &errorMessage);
    
    // DTK OCR engine
    QSharedPointer<Dtk::Ocr::DOcr> ocrEngine;
    
    // Supported capabilities
    QStringList supportedLanguages;
    QStringList supportedFormats;
    QString defaultLanguage;
    
    // Thread safety
    mutable QMutex engineMutex;
    bool initialized = false;
};

AIDAEMON_END_NAMESPACE

#endif // DTKOCR_H
