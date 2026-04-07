// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOUBALPROVIDER_P_H
#define DOUBALPROVIDER_P_H

#include "doubaoprovider.h"

AIDAEMON_BEGIN_NAMESPACE

class DouBaoProviderPrivate
{
public:
    DouBaoProviderPrivate(DouBaoProvider *parent);
private:
    DouBaoProvider *q =nullptr;
};


AIDAEMON_END_NAMESPACE

#endif // DOUBALMODELPROVIDER_P_H
