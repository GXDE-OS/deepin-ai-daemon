// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELCENTER_DEFINE_H
#define MODELCENTER_DEFINE_H

#include "aidaemon_global.h"

#include <QString>

#include <functional>

AIDAEMON_BEGIN_NAMESPACE

enum InterfaceType
{
    Chat = 0
};

struct ChatHistory
{
    QString role;
    QString content;
};

inline constexpr char kChatParamsStream[] { "stream" };
inline constexpr char kChatParamsMessages[] { "messages" };

using ChatStreamer = std::function<bool(const QString &, void *)>;
AIDAEMON_END_NAMESPACE

#endif // MODELCENTER_DEFINE_H
