// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ERNIEPROVIDER_H
#define ERNIEPROVIDER_H

#include "modelcenter/providers/modelprovider.h"

AIDAEMON_BEGIN_NAMESPACE
class ChatInterface;
class FunctionCallingInterface;
class ErnieProviderPrivate;
class ErnieProvider : public ModelProvider
{
    friend class ErnieProviderPrivate;
public:
    static inline QString providerName() {
        return "ernie";
    }
    static ErnieProvider *create();
    ErnieProvider();
    ~ErnieProvider();
public:
    QString name() override;
    AbstractInterface *createInterface(InterfaceType type) override;
    AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) override;
protected:
    ChatInterface *createChat(const QVariantHash &modelInfo);
    FunctionCallingInterface *createFunctionCalling(const QVariantHash &modelInfo);
private:
    ErnieProviderPrivate *d = nullptr;
};

AIDAEMON_END_NAMESPACE

#endif // ERNIEPROVIDER_H
