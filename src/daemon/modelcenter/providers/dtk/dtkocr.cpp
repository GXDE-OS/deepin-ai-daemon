// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dtkocr.h"
#include <docr.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMutexLocker>

AIDAEMON_USE_NAMESPACE

Q_LOGGING_CATEGORY(logDTKOCR, "ai.daemon.dtk.ocr")

DTKOCRInterface::DTKOCRInterface(const QString &model, QObject *parent)
    : OCRInterface(model, parent)
{
}

DTKOCRInterface::~DTKOCRInterface()
{
    cleanup();
    qCDebug(logDTKOCR) << "DTK OCR interface destroyed";
}

bool DTKOCRInterface::initialize()
{
    QMutexLocker locker(&engineMutex);
    
    if (initialized) {
        return true;
    }
    
    if (!setupOCREngine()) {
        qCWarning(logDTKOCR) << "Failed to setup OCR engine";
        return false;
    }
    
    initialized = true;
    qCInfo(logDTKOCR) << "DTK OCR interface initialized successfully";
    return true;
}

void DTKOCRInterface::cleanup()
{
    QMutexLocker locker(&engineMutex);
    
    if (!initialized) {
        return;
    }
    
    if (ocrEngine) {
        ocrEngine.reset();
    }
    
    initialized = false;
    qCInfo(logDTKOCR) << "DTK OCR interface cleaned up";
}

bool DTKOCRInterface::setupOCREngine()
{
    try {
        ocrEngine = QSharedPointer<Dtk::Ocr::DOcr>(new Dtk::Ocr::DOcr);
        
        // Load OCR plugin
        bool loaded = ocrEngine->loadDefaultPlugin();
        if (!loaded) {
            qCWarning(logDTKOCR) << "Failed to load default OCR plugin";
            return false;
        }
        
        // Set hardware acceleration if available
        auto hardwareList = ocrEngine->hardwareSupportList();
        if (!hardwareList.isEmpty()) {
            // Prefer GPU if available, otherwise use CPU
            QList<QPair<Dtk::Ocr::HardwareID, int>> hardwareUsed;
            bool foundGPU = false;
            for (const auto &hw : hardwareList) {
                if (hw >= Dtk::Ocr::GPU_Any && hw < GPU_HARDWARE_THRESHOLD && !foundGPU) {
                    hardwareUsed.append({hw, DEFAULT_HARDWARE_PRIORITY});
                    foundGPU = true;
                    qCInfo(logDTKOCR) << "Using GPU acceleration:" << hw;
                    break;
                }
            }
            if (!foundGPU && !hardwareList.isEmpty()) {
                hardwareUsed.append({hardwareList.first(), DEFAULT_HARDWARE_PRIORITY});
                qCInfo(logDTKOCR) << "Using hardware:" << hardwareList.first();
            }
            if (!hardwareUsed.isEmpty()) {
                ocrEngine->setUseHardware(hardwareUsed);
            }
        }
        
        // Initialize supported capabilities
        supportedLanguages = ocrEngine->languageSupport();
        if (supportedLanguages.isEmpty()) {
            supportedLanguages = QStringList{"zh-Hans_en", "zh-Hant_en", "en"};
        }
        defaultLanguage = supportedLanguages.contains("zh-Hans_en") ? "zh-Hans_en" : supportedLanguages.first();
        
        supportedFormats = ocrEngine->imageFileSupportFormats();

        return true;
        
    } catch (const std::exception &e) {
        qCWarning(logDTKOCR) << "Exception in setupOCREngine:" << e.what();
        return false;
    } catch (...) {
        qCWarning(logDTKOCR) << "Unknown exception in setupOCREngine";
        return false;
    }
}

QJsonObject DTKOCRInterface::recognizeFile(const QString &imageFile, const QVariantHash &params)
{
    qCDebug(logDTKOCR) << "Recognizing file:" << imageFile << "with params:" << params;
    
    if (!initialize()) {
        return createErrorResult(ERROR_CODE_NOT_INITIALIZED, "OCR engine not initialized");
    }
    
    if (!QFileInfo::exists(imageFile)) {
        return createErrorResult(ERROR_CODE_INVALID_INPUT, "Image file does not exist");
    }
    
    try {
        QString text = extractTextFromImage(imageFile, QRect(), params);
        return formatOCRResult(text, params);
    } catch (const std::exception &e) {
        return createErrorResult(ERROR_CODE_RECOGNITION_FAILED, QString("OCR recognition failed: %1").arg(e.what()));
    }
}

QJsonObject DTKOCRInterface::recognizeImage(const QByteArray &imageData, const QVariantHash &params)
{
    qCDebug(logDTKOCR) << "Recognizing image data, size:" << imageData.size() << "with params:" << params;
    
    if (!initialize()) {
        return createErrorResult(ERROR_CODE_NOT_INITIALIZED, "OCR engine not initialized");
    }
    
    if (imageData.isEmpty()) {
        return createErrorResult(ERROR_CODE_INVALID_INPUT, "Empty image data");
    }
    
    try {
        QString text = extractTextFromImageData(imageData, params);
        return formatOCRResult(text, params);
    } catch (const std::exception &e) {
        return createErrorResult(ERROR_CODE_RECOGNITION_FAILED, QString("OCR recognition failed: %1").arg(e.what()));
    }
}

QJsonObject DTKOCRInterface::recognizeRegion(const QString &imageFile, const QRect &region, const QVariantHash &params)
{
    qCDebug(logDTKOCR) << "Recognizing region:" << region << "in file:" << imageFile << "with params:" << params;
    
    if (!initialize()) {
        return createErrorResult(ERROR_CODE_NOT_INITIALIZED, "OCR engine not initialized");
    }
    
    if (!QFileInfo::exists(imageFile)) {
        return createErrorResult(ERROR_CODE_INVALID_INPUT, "Image file does not exist");
    }
    
    if (!region.isValid()) {
        return createErrorResult(ERROR_CODE_INVALID_INPUT, "Invalid region specified");
    }
    
    try {
        QString text = extractTextFromImage(imageFile, region, params);
        return formatOCRResult(text, params);
    } catch (const std::exception &e) {
        return createErrorResult(ERROR_CODE_RECOGNITION_FAILED, QString("OCR recognition failed: %1").arg(e.what()));
    }
}

QString DTKOCRInterface::extractTextFromImage(const QString &imageFile, const QRect &region, const QVariantHash &params)
{
    QMutexLocker locker(&engineMutex);
    
    if (!ocrEngine || !ocrEngine->pluginReady()) {
        throw std::runtime_error("OCR engine not ready");
    }
    
    // Set language
    QString language = params.value("language", defaultLanguage).toString();
    if (supportedLanguages.contains(language)) {
        ocrEngine->setLanguage(language);
    }
    
    // Load image
    QImage image(imageFile);
    if (image.isNull()) {
        throw std::runtime_error("Failed to load image file");
    }
    
    // Apply region if specified
    if (region.isValid()) {
        image = image.copy(region);
    }
    
    // Perform OCR
    bool success = ocrEngine->setImage(image);
    if (!success) {
        throw std::runtime_error("Failed to set image for OCR");
    }
    
    success = ocrEngine->analyze();
    if (!success) {
        throw std::runtime_error("OCR analysis failed");
    }
    
    return ocrEngine->simpleResult();
}

QString DTKOCRInterface::extractTextFromImageData(const QByteArray &imageData, const QVariantHash &params)
{
    QMutexLocker locker(&engineMutex);
    
    if (!ocrEngine || !ocrEngine->pluginReady()) {
        throw std::runtime_error("OCR engine not ready");
    }
    
    // Set language
    QString language = params.value("language", defaultLanguage).toString();
    if (supportedLanguages.contains(language)) {
        ocrEngine->setLanguage(language);
    }
    
    // Load image from data
    QImage image;
    if (!image.loadFromData(imageData)) {
        throw std::runtime_error("Failed to load image from data");
    }
    
    // Perform OCR
    bool success = ocrEngine->setImage(image);
    if (!success) {
        throw std::runtime_error("Failed to set image for OCR");
    }
    
    success = ocrEngine->analyze();
    if (!success) {
        throw std::runtime_error("OCR analysis failed");
    }
    
    return ocrEngine->simpleResult();
}

QJsonObject DTKOCRInterface::formatOCRResult(const QString &text, const QVariantHash &params)
{
    QJsonObject result;
    result["text"] = text;
    result["confidence"] = DEFAULT_CONFIDENCE_SCORE; // DTK OCR doesn't provide confidence, use default
    result["language"] = params.value("language", defaultLanguage).toString();
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Add detailed results if requested
    bool includeDetails = params.value("includeDetails", false).toBool();
    if (includeDetails && ocrEngine) {
        QMutexLocker locker(&engineMutex);
        QJsonArray textBoxes;
        auto boxes = ocrEngine->textBoxes();
        for (int i = 0; i < boxes.size(); ++i) {
            const auto &box = boxes[i];
            QString boxText = ocrEngine->resultFromBox(i);
            
            QJsonObject boxInfo;
            boxInfo["text"] = boxText;
            boxInfo["confidence"] = DEFAULT_CONFIDENCE_SCORE;
            
            // Convert TextBox points to bounding box
            QJsonArray points;
            for (const auto &point : box.points) {
                QJsonArray pointArray;
                pointArray.append(point.x());
                pointArray.append(point.y());
                points.append(pointArray);
            }
            boxInfo["points"] = points;
            boxInfo["angle"] = box.angle;
            
            textBoxes.append(boxInfo);
        }
        result["textBoxes"] = textBoxes;
    }
    
    return result;
}

QJsonObject DTKOCRInterface::createErrorResult(int errorCode, const QString &errorMessage)
{
    QJsonObject result;
    result["error"] = true;
    result["error_code"] = errorCode;
    result["error_message"] = errorMessage;
    result["text"] = "";
    return result;
}

QStringList DTKOCRInterface::getSupportedLanguages() const
{
    return supportedLanguages;
}

QStringList DTKOCRInterface::getSupportedFormats() const
{
    return supportedFormats;
}

QJsonObject DTKOCRInterface::getCapabilities() const
{
    QJsonObject capabilities;
    capabilities["supportedLanguages"] = QJsonArray::fromStringList(supportedLanguages);
    capabilities["supportedFormats"] = QJsonArray::fromStringList(supportedFormats);
    capabilities["supportsRegion"] = true;
    capabilities["supportsProgress"] = true;
    capabilities["hardwareAcceleration"] = ocrEngine ? true : false;
    capabilities["maxImageSize"] = MAX_IMAGE_SIZE_BYTES;
    
    return capabilities;
}

void DTKOCRInterface::terminate()
{
    cleanup();
}
