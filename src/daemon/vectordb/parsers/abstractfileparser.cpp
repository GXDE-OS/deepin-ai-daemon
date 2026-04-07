// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "abstractfileparser.h"
#include "utils/utils.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpression>


AIDAEMON_VECTORDB_BEGIN_NAMESPACE

AbstractFileParser::AbstractFileParser(QObject *parent)
    : QObject(parent)
{
}

bool AbstractFileParser::canParse(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }
    
    // Check by extension first
    QString extension = fileInfo.suffix();
    if (!extension.isEmpty() && canParseExtension(extension)) {
        return true;
    }
    
    // Check by MIME type
    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    if (mimeType.isValid() && canParseMimeType(mimeType.name())) {
        return true;
    }
    
    return false;
}

bool AbstractFileParser::canParseMimeType(const QString &mimeType) const
{
    return Utils::matchPattern(mimeType, supportedMimeTypes()) > 0;
}

bool AbstractFileParser::canParseExtension(const QString &extension) const
{
    // Remove leading dot if present for consistent matching
    QString cleanExt = extension.startsWith('.') ? extension.mid(1) : extension;
    return Utils::matchPattern(cleanExt, supportedExtensions()) > 0;
}

float AbstractFileParser::getPriority(const QString &extension, const QString &mimeType) const
{
    float priority = 0;
    
    if (!extension.isEmpty()) {
        // Remove leading dot if present for consistent matching
        QString cleanExt = extension.startsWith('.') ? extension.mid(1) : extension;
        float extWeight = Utils::matchPattern(cleanExt, supportedExtensions());
        priority += extWeight;
    }

    if (!mimeType.isEmpty()) {
        float mimeWeight = Utils::matchPattern(mimeType, supportedMimeTypes());
        priority += mimeWeight;
    }

    return priority;
}

AIDAEMON_VECTORDB_END_NAMESPACE
