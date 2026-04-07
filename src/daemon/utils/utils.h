// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UTILS_H
#define UTILS_H

#include <QObject>

class Utils : public QObject
{
    Q_OBJECT

public:
    explicit Utils(QObject *parent = nullptr);

    static QString textEncodingTransferUTF8(const std::string &content);
    static bool isValidContent(const std::string &content);

    /**
     * @brief Check if a string matches any of the supported patterns using regex
     * @param input String to check
     * @param patterns Hash of patterns with their weights
     * @return Weight of the best matching pattern, or 0 if no match
     */
    static float matchPattern(const QString &input, const QHash<QString, float> &patterns);
};

#endif // UTILS_H
