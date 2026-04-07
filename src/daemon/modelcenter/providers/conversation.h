// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONVERSATION_H
#define CONVERSATION_H

#include <QJsonObject>
#include <QJsonArray>
#include "modelcenter/modelcenter_define.h"

AIDAEMON_BEGIN_NAMESPACE

class Conversation
{
public:
    Conversation();
    virtual ~Conversation();

    /**
     * @brief conversationLastUserData
     * @param conversation
     * @return
     */
    static QString conversationLastUserData(const QString &conversation);

    virtual void fromHistory(const QList<ChatHistory> &history);
public:
    bool setSystemData(const QString &data);
    bool popSystemData();

    bool addUserData(const QString &data);
    bool popUserData();

    bool addAssistantData(const QString &data);
    bool popAssistantData();

    QString getLastResponse() const;
    QByteArray getLastByteResponse() const;
    bool popLastResponse();

    QJsonArray getLastTools() const;
    bool popLastTools();

    bool setFunctions(const QJsonArray &functions);
    QJsonArray getFunctions() const;
    QJsonArray getFunctionTools() const;

    QJsonArray getConversions() const;

protected:
    QJsonArray m_conversion;
    QJsonArray m_functions;
};

AIDAEMON_END_NAMESPACE

#endif // CONVERSATION_H
