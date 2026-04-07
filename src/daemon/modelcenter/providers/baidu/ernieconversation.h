// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ERNIECONVERSATION_H
#define ERNIECONVERSATION_H

#include "modelcenter/providers/conversation.h"

AIDAEMON_BEGIN_NAMESPACE

class ErnieConversation : public Conversation
{
public:
    ErnieConversation();
public:
    void update(const QByteArray &response);

public:
    QJsonObject parseContentString(const QString &content);
protected:
   QByteArray m_deltacontent;
};

AIDAEMON_END_NAMESPACE

#endif // ERNIECONVERSATION_H
