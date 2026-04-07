// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOUBAOCHAT_H
#define DOUBAOCHAT_H

#include "modelcenter/providers/openai/universalchatinterface.h"

AIDAEMON_BEGIN_NAMESPACE

namespace doubao {

class DouBaoChat : public UniversalChatInterface
{
public:
    DouBaoChat(const QString &model, QObject *parent = nullptr);
};

}

AIDAEMON_END_NAMESPACE

#endif // DOUBAOCHAT_H
