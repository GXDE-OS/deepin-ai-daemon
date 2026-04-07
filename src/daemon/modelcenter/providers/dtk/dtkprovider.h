// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DTKPROVIDER_H
#define DTKPROVIDER_H

#include "modelcenter/providers/modelprovider.h"

#include <QObject>
#include <QStringList>
#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

class OCRInterface;

class DTKProvider : public ModelProvider
{
public:
    static inline QString providerName() {
        return "dtk";
    }
    static DTKProvider *create();
    
    DTKProvider();
    ~DTKProvider() override;
    
    // ModelProvider interface
    QString name() override;
    AbstractInterface *createInterface(InterfaceType type) override;
    AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) override;
    
protected:
    OCRInterface *createOCR(const QVariantHash &modelInfo);
};

AIDAEMON_END_NAMESPACE

#endif // DTKPROVIDER_H
