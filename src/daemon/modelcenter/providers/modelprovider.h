// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELPROVIDER_H
#define MODELPROVIDER_H

#include "modelcenter/modelcenter_define.h"

#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

class AbstractInterface;
class ModelProvider
{
public:
    ModelProvider();
    virtual ~ModelProvider();
    virtual QString name() = 0;
    virtual AbstractInterface *createInterface(InterfaceType type) = 0;
    virtual AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) = 0;
};

AIDAEMON_END_NAMESPACE

#endif // MODELPROVIDER_H
