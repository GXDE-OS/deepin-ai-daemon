// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATINTERFACE_H
#define CHATINTERFACE_H

#include "abstractinterface.h"

AIDAEMON_BEGIN_NAMESPACE

class ChatInterface : public AbstractInterface
{
public:
    ChatInterface();
    InterfaceType type() override;
};

AIDAEMON_END_NAMESPACE

#endif // CHATINTERFACE_H
