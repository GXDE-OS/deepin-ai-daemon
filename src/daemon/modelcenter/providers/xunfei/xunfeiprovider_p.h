// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEIPROVIDER_P_H
#define XUNFEIPROVIDER_P_H

#include "xunfeiprovider.h"

AIDAEMON_BEGIN_NAMESPACE

class XunfeiProviderPrivate
{
public:
    explicit XunfeiProviderPrivate(XunfeiProvider *parent);
    
    XunfeiProvider *q;
};

AIDAEMON_END_NAMESPACE

#endif // XUNFEIPROVIDER_P_H 