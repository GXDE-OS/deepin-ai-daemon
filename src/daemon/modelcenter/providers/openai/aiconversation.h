// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AICONVERSATION_H
#define AICONVERSATION_H

#include "modelcenter/providers/conversation.h"

AIDAEMON_BEGIN_NAMESPACE

class AIConversation : public Conversation
{
public:
    AIConversation();
public:
    void update(const QByteArray &response);

public:
    static QJsonObject parseContentString(const QString &content);
};

AIDAEMON_END_NAMESPACE

#endif // CONVERSATION_H
