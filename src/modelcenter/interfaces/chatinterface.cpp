// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatinterface.h"

AIDAEMON_USE_NAMESPACE

ChatInterface::ChatInterface() : AbstractInterface()
{

}

InterfaceType ChatInterface::type()
{
    return Chat;
}
