// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ernieconversation.h"

#include <QDebug>
#include <QJsonDocument>
#include <QRegularExpression>

AIDAEMON_USE_NAMESPACE

ErnieConversation::ErnieConversation()
{

}

void ErnieConversation::update(const QByteArray &response)
{
    if (response.isEmpty())
        return;

    if (response.startsWith("data:")) {
        const QJsonObject &delateData = parseContentString(response);
        if (delateData.contains("content")) {
            m_conversion.push_back(QJsonObject({
                { "role",    "assistant"    },
                { "content", delateData.value("content") }
            }));
        }

        if (delateData.contains("tools")) {
            m_conversion.push_back(QJsonObject({
                { "role",       "tools"    },
                { "tool_calls",    delateData.value("tool_calls") }
            }));
        }
    } else {
        const QJsonObject &root = QJsonDocument::fromJson(response).object();
        QString deltacontent;
        if (root.contains("result"))
            deltacontent = root.value("result").toString();

        m_conversion.push_back(QJsonObject({
            { "role",    "assistant"    },
            { "content", deltacontent }
        }));

        if (root.contains("function_call")) {
            QJsonObject funcValue =  root.value("function_call").toObject();
            QJsonObject functionCall;
            functionCall.insert("name", funcValue.value("name"));
            functionCall.insert("arguments", funcValue.value("arguments"));

            QJsonArray toolCalls;
            QJsonObject func;
            func.insert("function", functionCall);
            func.insert("type", "function");
            toolCalls.append(func);

            m_conversion.push_back(QJsonObject({
                { "role",       "tools"    },
                { "tool_calls", toolCalls }
            }));
        }
    }
}

QJsonObject ErnieConversation::parseContentString(const QString &content)
{
    m_deltacontent += content.toUtf8();
    if (!content.trimmed().endsWith("}")) {
        return QJsonObject();
    }

    QRegularExpression regex(R"(data:\s*\{(.*)\})");
    QRegularExpressionMatchIterator iter = regex.globalMatch(m_deltacontent);

    QByteArray cacheDeltacontent;
    QJsonObject functionCall;
    QMap<int, QString> seqContents;

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString matchString = match.captured(0);

        int startIndex = matchString.indexOf('{');
        int endIndex = matchString.lastIndexOf('}');

        if (startIndex >= 0 && endIndex > startIndex) {
            QString content = matchString.mid(startIndex, endIndex - startIndex + 1);

            QJsonObject j = QJsonDocument::fromJson(content.toUtf8()).object();
            if (j.isEmpty()) {
                cacheDeltacontent += matchString.toUtf8();
            } else {
                if (j.contains("result")) {
                    seqContents[j.value("sentence_id").toInt()] += j.value("result").toString();
                }

                if (j.contains("function_call")) {
                    const QJsonObject &function_call =  j.value("function_call").toObject();
                    if (function_call.contains("name")) {
                        functionCall["name"] = functionCall["name"].toString() + function_call.value("name").toString();
                    }

                    if (function_call.contains("arguments")) {
                        functionCall["arguments"] = functionCall["arguments"].toString() + function_call.value("arguments").toString();
                    }
                }
            }
        }
    }

    m_deltacontent.clear();
    m_deltacontent += cacheDeltacontent;

    QString deltacontent;
    for (auto iter = seqContents.begin(); iter != seqContents.end(); iter++) {
        deltacontent += iter.value();
    }

    QJsonObject response;
    if (!deltacontent.isEmpty())
        response["content"] = deltacontent;

    if (!functionCall.isEmpty()) {
        QJsonArray toolCalls;
        QJsonObject func;
        func.insert("function", functionCall);
        func.insert("type", "function");
        toolCalls.append(func);
        response["tool_calls"] = toolCalls;
    }

    return response;
}
