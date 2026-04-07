// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocrparser.h"
#include "parsers/textblobresult.h"

#include <DOcr>
#include <QImage>
#include <QMutexLocker>
#include <QDebug>

DOCR_USE_NAMESPACE

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

OcrParser::OcrParser(QObject *parent)
    : AbstractFileParser(parent)
{
}

OcrParser::~OcrParser()
{
}

AbstractFileParser *OcrParser::create()
{
    return new OcrParser;
}

QString OcrParser::name() const
{
    return parserName();
}

QString OcrParser::description() const
{
    return "Extracts text from images using OCR (Optical Character Recognition) technology";
}

QHash<QString, float> OcrParser::supportedMimeTypes() const
{
    QHash<QString, float> mimeTypes;
    mimeTypes["image/jpeg"] = 1.0f;
    mimeTypes["image/jpg"] = 1.0f;
    mimeTypes["image/png"] = 1.0f;
    mimeTypes["image/bmp"] = 0.9f;
    mimeTypes["image/tiff"] = 0.9f;
    mimeTypes["image/gif"] = 0.8f;
    mimeTypes["image/webp"] = 0.8f;
    return mimeTypes;
}

QHash<QString, float> OcrParser::supportedExtensions() const
{
    QHash<QString, float> extensions;
    extensions["jpg"] = 1.0f;
    extensions["jpeg"] = 1.0f;
    extensions["png"] = 1.0f;
    extensions["bmp"] = 0.9f;
    extensions["tiff"] = 0.9f;
    extensions["tif"] = 0.9f;
    extensions["gif"] = 0.8f;
    extensions["webp"] = 0.8f;
    return extensions;
}

QSharedPointer<ParserResult> OcrParser::parseFile(const QString &filePath, const QVariantMap &metadata)
{
    Q_UNUSED(metadata)
    
    auto result = QSharedPointer<TextBlobResult>::create();
    
    // Load image
    QImage image(filePath);
    if (image.isNull()) {
        result->setStatus(-1);
        result->setErrorString(QString("Failed to load image: %1").arg(filePath));
        return qSharedPointerDynamicCast<ParserResult>(result);
    }
    
    // Scale down high resolution images to prevent memory issues during OCR
    if (image.width() > kImageSizeLimit || image.height() > kImageSizeLimit) {
        image = image.width() > image.height()
                ? image.scaledToWidth(kImageSizeLimit, Qt::SmoothTransformation)
                : image.scaledToHeight(kImageSizeLimit, Qt::SmoothTransformation);
    }
    
    // Perform OCR
    {
        QMutexLocker lk(&m_mtx);
        if (m_ocr.isNull()) {
            m_ocr.reset(new DOcr);
            if (!m_ocr->loadDefaultPlugin()) {
                result->setStatus(-1);
                result->setErrorString("Failed to load OCR plugin");
                return qSharedPointerDynamicCast<ParserResult>(result);
            }
            m_ocr->setLanguage("zh-Hans_en");
        }
        
        m_ocr->setImage(image);
        if (m_ocr->analyze()) {
            const QString &ocrResult = m_ocr->simpleResult();
            if (!ocrResult.isEmpty()) {
                result->setContent(ocrResult);
                result->setStatus(0);
            } else {
                result->setStatus(-1);
                result->setErrorString("OCR analysis completed but no text was found");
            }
        } else {
            result->setStatus(-1);
            result->setErrorString("OCR analysis failed");
        }
    }
    
    return qSharedPointerDynamicCast<ParserResult>(result);
}

AIDAEMON_VECTORDB_END_NAMESPACE
