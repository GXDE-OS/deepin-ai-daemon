// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OPENAIPROVIDER_P_H
#define OPENAIPROVIDER_P_H

#include "openaiprovider.h"

AIDAEMON_BEGIN_NAMESPACE

class OpenAIProviderPrivate
{
public:
    OpenAIProviderPrivate(OpenAIProvider *parent);
private:
    OpenAIProvider *q = nullptr;
};


AIDAEMON_END_NAMESPACE

#endif // OPENAIPROVIDER_P_H
