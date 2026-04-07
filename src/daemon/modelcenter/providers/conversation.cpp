// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conversation.h"

#include <QJsonDocument>
#include <QVariant>

AIDAEMON_USE_NAMESPACE
Conversation::Conversation()
{

}

Conversation::~Conversation()
{

}

QString Conversation::conversationLastUserData(const QString &conversation)
{
    const QJsonArray &array = QJsonDocument::fromJson(conversation.toUtf8()).array();
    if (!array.isEmpty() && array.last()["role"] == "user") {
        return array.last()["content"].toString();
    }

    return conversation;
}

void Conversation::fromHistory(const QList<ChatHistory> &history)
{
    m_conversion = QJsonArray();

    for (ChatHistory chathis : history) {
        if (chathis.role == "system") {
            setSystemData(chathis.content);
        } else if (chathis.role == "user") {
            addUserData(chathis.content);
        } else if (chathis.role == "assistant") {
            addAssistantData(chathis.content);
        }
    }
}

bool Conversation::setSystemData(const QString &data)
{
    if (!data.isEmpty()) {
        for (auto iter = m_conversion.begin(); iter != m_conversion.end(); iter++) {
            if (iter->toObject().value("role").toString() == "system")
                return false;
        }
        m_conversion.insert(0, QJsonObject({ { "role", "system" }, {"content", data} }));
        return true;
    }
    return false;
}

bool Conversation::popSystemData()
{
    if (!m_conversion.isEmpty() && m_conversion.at(0)["role"].toString() == "system") {
        m_conversion.removeFirst();
        return true;
    }
    return false;
}

bool Conversation::addUserData(const QString &data)
{
    if (!data.isEmpty()) {
        m_conversion.push_back(QJsonObject({ { "role", "user" }, {"content", data} }));
        return true;
    }
    return false;
}

bool Conversation::popUserData()
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "user") {
        m_conversion.removeLast();
        return true;
    }
    return false;
}

bool Conversation::addAssistantData(const QString &data)
{
    if (!data.isEmpty()) {
        m_conversion.push_back(QJsonObject({ { "role", "assistant" }, {"content", data} }));
        return true;
    }
    return false;
}

bool Conversation::popAssistantData()
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "assistant") {
        m_conversion.removeLast();
        return true;
    }
    return false;
}

QString Conversation::getLastResponse() const
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "assistant") {
        return m_conversion.last()["content"].toString();
    }
    return QString();
}

QByteArray Conversation::getLastByteResponse() const
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "assistant") {
        return m_conversion.last()["content"].toVariant().toByteArray();
    }
    return QByteArray();
}

bool Conversation::popLastResponse()
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "assistant") {
        m_conversion.removeLast();
        return true;
    }
    return false;
}

QJsonArray Conversation::getLastTools() const
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "tools") {
        return m_conversion.last()["tool_calls"].toArray();
    }

    return QJsonArray();
}

bool Conversation::popLastTools()
{
    if (!m_conversion.isEmpty() && m_conversion.last()["role"].toString() == "tools") {
        m_conversion.removeLast();
        return true;
    }
    return false;
}

bool Conversation::setFunctions(const QJsonArray &functions)
{    
    m_functions = functions;
    return true;
}

QJsonArray Conversation::getConversions() const
{
    return m_conversion;
}

QJsonArray Conversation::getFunctions() const
{
    return m_functions;
}

QJsonArray Conversation::getFunctionTools() const
{
    QJsonArray tools;
    for (const QJsonValue &fun : m_functions) {
        QJsonObject tool;
        tool["type"] = "function";
        tool["function"] = fun;
        tools << tool;
    }

    return tools;
}
