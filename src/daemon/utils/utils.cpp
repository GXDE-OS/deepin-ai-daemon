// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "utils.h"

#include <DTextEncoding>

#include <QRegularExpression>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logUtils)

Utils::Utils(QObject *parent) : QObject(parent)
{

}

QString Utils::textEncodingTransferUTF8(const std::string &content)
{
    if (content.empty())
        return "";

    QByteArray data = QByteArray::fromStdString(content);
    QString codeFormat = Dtk::Core::DTextEncoding::detectTextEncoding(data);
    if (codeFormat.toLower() == QString("utf-8"))
        return QString::fromStdString(content);

    QByteArray out;
    if (Dtk::Core::DTextEncoding::convertTextEncoding(data, out, "utf-8", codeFormat.toUtf8())) {
        return QString::fromUtf8(out);
    } else {
        qCWarning(logUtils) << "Failed to convert encoding from" << codeFormat << "to UTF-8";
        return "";
    }
}

bool Utils::isValidContent(const std::string &content)
{
    QByteArray data = QByteArray::fromStdString(content);

    QMimeDatabase mimeDB;
    QMimeType mimeType = mimeDB.mimeTypeForData(data);
    if (!mimeType.isValid()) {
        qCDebug(logUtils) << "Invalid mime type detected for content";
        return false;
    }

    if (mimeType.name().contains("text")) {
        qCDebug(logUtils) << "Valid text content detected, mime type:" << mimeType.name();
        return true;
    }

    qCDebug(logUtils) << "Non-text content detected, mime type:" << mimeType.name();
    return false;
}

float Utils::matchPattern(const QString &input, const QHash<QString, float> &patterns)
{
    float bestWeight = 0.0f;

    for (auto it = patterns.begin(); it != patterns.end(); ++it) {
        const QString &pattern = it.key();
        float weight = it.value();

        // First try exact match
        if (pattern.compare(input, Qt::CaseInsensitive) == 0) {
            if (weight > bestWeight) {
                bestWeight = weight;
                continue;
            }
        }

        // Then try regex match
        QRegularExpression regex(pattern);
        if (regex.isValid() && regex.match(input).hasMatch()) {
            if (weight > bestWeight) {
                bestWeight = weight;
            }
        }
    }

    return bestWeight;
}
