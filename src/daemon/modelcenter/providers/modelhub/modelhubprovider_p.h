// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELHUBPROVIDER_P_H
#define MODELHUBPROVIDER_P_H

#include "modelhubprovider.h"
#include "modelcenter/modelhub/modelhubwrapper.h"
#include <QMap>

AIDAEMON_BEGIN_NAMESPACE

class ModelhubProviderPrivate
{
public:
    ModelhubProviderPrivate(ModelhubProvider *parent);
    QSharedPointer<ModelhubWrapper> instance(const QString &name);
    QMap<QString , QSharedPointer<ModelhubWrapper>> m_wrapperMap;
private:
    ModelhubProvider *q =nullptr;
};


AIDAEMON_END_NAMESPACE

#endif // MODELHUBPROVIDER_P_H
