// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doubaochat.h"

AIDAEMON_USE_NAMESPACE

using namespace doubao;

DouBaoChat::DouBaoChat(const QString &model, QObject *parent)
    : UniversalChatInterface(model, parent)
{
    m_apiUrl = "https://ark.cn-beijing.volces.com/api/v3/chat/completions";
}
