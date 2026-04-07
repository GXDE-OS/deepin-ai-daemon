// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OCRPARSER_H
#define OCRPARSER_H

#include "aidaemon_global.h"
#include "parsers/abstractfileparser.h"

#include <QMutex>
#include <QScopedPointer>

namespace Dtk {
namespace Ocr {
class DOcr;
}
}

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief OCR parser for extracting text from images
 * 
 * Parses image files and extracts text content using OCR technology.
 * Supports various image formats including JPG, PNG, BMP, etc.
 */
class OcrParser : public AbstractFileParser
{
    Q_OBJECT

public:
    explicit OcrParser(QObject *parent = nullptr);
    ~OcrParser();
    
    static AbstractFileParser *create();
    static inline QString parserName() {
        return "default-ocr";
    }
    QString name() const override;
    QString description() const override;
    QHash<QString, float> supportedMimeTypes() const override;
    QHash<QString, float> supportedExtensions() const override;
    QSharedPointer<ParserResult> parseFile(const QString &filePath, const QVariantMap &metadata = QVariantMap()) override;

private:
    mutable QMutex m_mtx;
    QSharedPointer<Dtk::Ocr::DOcr> m_ocr;
    static constexpr int kImageSizeLimit = 1024;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // OCRPARSER_H
