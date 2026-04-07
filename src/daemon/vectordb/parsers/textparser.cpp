// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textparser.h"
#include "parsers/textblobresult.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

TextParser::TextParser(QObject *parent)
    : AbstractFileParser(parent)
{
}

AbstractFileParser *TextParser::create()
{
    return new TextParser;
}

QString TextParser::name() const
{
    return parserName();
}

QString TextParser::description() const
{
    return "Parses plain text files with automatic encoding detection";
}

QHash<QString, float> TextParser::supportedMimeTypes() const
{
    QHash<QString, float> mimeTypes;
    mimeTypes["text/plain"] = 1.0f;
    mimeTypes["text/txt"] = 0.9f;
    mimeTypes["application/x-empty"] = 0.1f;
    return mimeTypes;
}

QHash<QString, float> TextParser::supportedExtensions() const
{
    QHash<QString, float> extensions;
    extensions["txt"] = 1.0f;
    extensions["text"] = 0.9f;
    extensions["log"] = 0.8f;
    extensions["md"] = 0.7f;
    extensions["markdown"] = 0.7f;
    extensions["conf"] = 0.7f;
    extensions["ini"] = 0.7f;
    extensions["json"] = 0.7f;
    extensions["rst"] = 0.6f;
    extensions["readme"] = 0.5f;
    extensions["me"] = 0.5f;
    extensions["nfo"] = 0.4f;
    extensions["info"] = 0.4f;
    return extensions;
}

QSharedPointer<ParserResult> TextParser::parseFile(const QString &filePath, const QVariantMap &metadata)
{
    auto result = QSharedPointer<TextBlobResult>::create();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QString errorMsg = QString("Failed to open file: %1").arg(file.errorString());
        result->setErrorString(errorMsg);
        result->setStatus(-1);
        return qSharedPointerDynamicCast<ParserResult>(result);
    }
    
    QByteArray data = file.readAll();

    // todo text decode
    QString content = QString::fromUtf8(data);
    file.close();
    
    result->setContent(content);
    return qSharedPointerDynamicCast<ParserResult>(result);
}

AIDAEMON_VECTORDB_END_NAMESPACE
