// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ERNIEPROVIDER_P_H
#define ERNIEPROVIDER_P_H

#include "ernieprovider.h"

AIDAEMON_BEGIN_NAMESPACE

class ErnieProviderPrivate
{
public:
    ErnieProviderPrivate(ErnieProvider *parent);
private:
    ErnieProvider *q =nullptr;
};


AIDAEMON_END_NAMESPACE

#endif // ERNIEPROVIDER_P_H
