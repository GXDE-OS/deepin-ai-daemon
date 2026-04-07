// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aiconversation.h"

#include <QJsonDocument>
#include <QRegularExpression>

AIDAEMON_USE_NAMESPACE

AIConversation::AIConversation()
{

}

QJsonObject AIConversation::parseContentString(const QString &content)
{
    QString deltacontent;

    QRegularExpression regex(R"(data:\s*\{(.*)\})");
    QRegularExpressionMatchIterator iter = regex.globalMatch(content);

    QMap<int, QJsonObject> toolCallMaps;

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString matchString = match.captured(0);

        int startIndex = matchString.indexOf('{');
        int endIndex = matchString.lastIndexOf('}');

        if (startIndex >= 0 && endIndex > startIndex) {
            QString content = matchString.mid(startIndex, endIndex - startIndex + 1);

            QJsonObject j = QJsonDocument::fromJson(content.toUtf8()).object();
            if (j.contains("choices")) {
                const QJsonArray &choices = j["choices"].toArray();
                for (auto choice = choices.begin(); choice != choices.end(); choice++) {
                    const QJsonObject &cj = choice->toObject();
                    if (cj.contains("delta")) {
                        const QJsonObject &delta = cj["delta"].toObject();
                        if (delta.contains("content")) {
                            const QString &deltaData = delta["content"].toString();
                            deltacontent += deltaData;
                        }

                        if (delta.contains("tool_calls")) {
                            const QJsonArray &tool_calls =  delta.value("tool_calls").toArray();
                            for (const QJsonValue &tool_call : tool_calls) {
                                const QJsonObject &toolCallObj = tool_call.toObject();

                                int index = toolCallObj["index"].toInt();
                                if (!toolCallMaps[index].contains("function")) {
                                    toolCallMaps[index]["function"] = QJsonObject();
                                }

                                toolCallMaps[index]["index"] = index;

                                if (toolCallObj.contains("id")) {
                                    toolCallMaps[index]["id"] = toolCallObj.value("id");
                                }

                                if (toolCallObj.contains("type")) {
                                    toolCallMaps[index]["type"] = toolCallObj.value("type");
                                }

                                if (toolCallObj.contains("function")) {
                                    QJsonObject toolFun = toolCallMaps[index]["function"].toObject();
                                    const QJsonObject &tmpToolFun =  toolCallObj.value("function").toObject();
                                    if (tmpToolFun.contains("name")) {
                                        toolFun["name"] = toolFun["name"].toString() + tmpToolFun.value("name").toString();
                                    }

                                    if (tmpToolFun.contains("arguments")) {
                                        toolFun["arguments"] = toolFun["arguments"].toString() + tmpToolFun.value("arguments").toString();
                                    }

                                    toolCallMaps[index]["function"] = toolFun;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    QJsonObject response;
    if (!deltacontent.isEmpty())
        response["content"] = deltacontent;

    QJsonArray toolCalls;
    for (auto iter = toolCallMaps.begin(); iter != toolCallMaps.end(); iter++) {
        toolCalls << iter.value();
    }

    if (!toolCalls.isEmpty())
        response["tool_calls"] = toolCalls;

    return response;
}

void AIConversation::update(const QByteArray &response)
{
    if (response.isEmpty())
        return;

    if (response.startsWith("data:")) {
        const QJsonObject &delateData = parseContentString(response);
        if (delateData.contains("content")) {
            m_conversion.push_back(QJsonObject({
                { "role",     "assistant"    },
                { "content",  delateData.value("content") }
            }));
        }

        if (delateData.contains("tool_calls")) {
            m_conversion.push_back(QJsonObject({
                { "role",       "tools"    },
                { "tool_calls",    delateData.value("tool_calls")}
            }));
        }
    } else {
        const QJsonObject &j = QJsonDocument::fromJson(response).object();
        if (j.contains("choices")) {
            const QJsonArray &choices = j["choices"].toArray();
            for (auto choice = choices.begin(); choice != choices.end(); choice++) {
                const QJsonObject &cj = choice->toObject();
                if (cj.contains("message")) {
                    QJsonObject mj = cj["message"].toObject();
                    if (!mj["role"].isNull() && !cj["content"].isNull()) {
                        m_conversion.push_back(QJsonObject({
                            { "role",    mj["role"]    },
                            { "content", mj["content"] }
                        }));
                    }

                    if (mj.contains("tool_calls")) {
                        QMap<int, QJsonObject> toolCallMaps;
                        const QJsonArray &tool_calls =  mj.value("tool_calls").toArray();
                        for (const QJsonValue &tool_call : tool_calls) {
                            const QJsonObject &toolCallObj = tool_call.toObject();

                            int index = toolCallObj["index"].toInt();
                            if (!toolCallMaps[index].contains("function")) {
                                toolCallMaps[index]["function"] = QJsonObject();
                            }

                            toolCallMaps[index]["index"] = index;

                            if (toolCallObj.contains("id")) {
                                toolCallMaps[index]["id"] = toolCallObj.value("id");
                            }

                            if (toolCallObj.contains("type")) {
                                toolCallMaps[index]["type"] = toolCallObj.value("type");
                            }

                            if (toolCallObj.contains("function")) {
                                QJsonObject toolFun = toolCallMaps[index]["function"].toObject();
                                const QJsonObject &tmpToolFun =  toolCallObj.value("function").toObject();
                                if (tmpToolFun.contains("name")) {
                                    toolFun["name"] = toolFun["name"].toString() + tmpToolFun.value("name").toString();
                                }

                                if (tmpToolFun.contains("arguments")) {
                                    toolFun["arguments"] = toolFun["arguments"].toString() + tmpToolFun.value("arguments").toString();
                                }

                                toolCallMaps[index]["function"] = toolFun;
                            }
                        }

                        QJsonArray toolCalls;
                        for (auto iter = toolCallMaps.begin(); iter != toolCallMaps.end(); iter++) {
                            toolCalls << iter.value();
                        }

                        if (!toolCalls.isEmpty()) {
                            m_conversion.push_back(QJsonObject({
                                { "role",       "tools"    },
                                { "tool_calls",    toolCalls }
                            }));
                        }
                    }
                }
            }
        }
    }
}
