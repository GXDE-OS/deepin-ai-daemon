// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pdfparser.h"
#include "parsers/textblobresult.h"
#include "utils/utils.h"

#include <QFile>
#include <QtCore/QDebug>

#include <docparser.h>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

PdfParser::PdfParser(QObject *parent)
    : AbstractFileParser(parent)
{
}

AbstractFileParser *PdfParser::create()
{
    return new PdfParser;
}

QString PdfParser::name() const
{
    return parserName();
}

QString PdfParser::description() const
{
    return "Extracts text content from PDF documents using external tools";
}

QHash<QString, float> PdfParser::supportedMimeTypes() const
{
    QHash<QString, float> mimeTypes;
    mimeTypes["application/pdf"] = 1.0f;
    return mimeTypes;
}

QHash<QString, float> PdfParser::supportedExtensions() const
{
    QHash<QString, float> extensions;
    extensions["pdf"] = 1.0f;
    return extensions;
}

QSharedPointer<ParserResult> PdfParser::parseFile(const QString &filePath, const QVariantMap &metadata)
{
    Q_UNUSED(metadata)

    auto ret = QSharedPointer<TextBlobResult>::create();

    std::string stdStrContents = DocParser::convertFile(filePath.toStdString());
    QString contents = Utils::textEncodingTransferUTF8(stdStrContents);

    if (!Utils::isValidContent(stdStrContents)) {
        ret->setStatus(-1);
        ret->setErrorString("Invalid document content.");
    } else {
        ret->setContent(contents);
    }

    return qSharedPointerDynamicCast<ParserResult>(ret);
}

AIDAEMON_VECTORDB_END_NAMESPACE
