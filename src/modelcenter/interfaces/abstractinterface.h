// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ABSTRACTINTERFACE_H
#define ABSTRACTINTERFACE_H

#include "modelcenter/modelcenter_define.h"

AIDAEMON_BEGIN_NAMESPACE

class AbstractInterface
{
public:
    AbstractInterface();
    virtual ~AbstractInterface();
    virtual InterfaceType type() = 0;
};

AIDAEMON_END_NAMESPACE

#endif // ABSTRACTINTERFACE_H
