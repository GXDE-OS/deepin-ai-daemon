// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELHUBPROVIDER_H
#define MODELHUBPROVIDER_H

#include "modelcenter/providers/modelprovider.h"

AIDAEMON_BEGIN_NAMESPACE
class ChatInterface;
class FunctionCallingInterface;
class EmbeddingInterface;
class ModelhubProviderPrivate;
class ModelhubProvider : public ModelProvider
{
    friend class ModelhubProviderPrivate;
public:
    static inline QString providerName() {
        return "modelhub";
    }
    static ModelhubProvider *create();

    ModelhubProvider();
    ~ModelhubProvider() override;
public:
    QString name() override;
    AbstractInterface *createInterface(InterfaceType type) override;
    AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) override;
protected:
    ChatInterface *createChat(const QVariantHash &modelInfo);
    FunctionCallingInterface *createFunctionCalling(const QVariantHash &modelInfo);
    EmbeddingInterface *createEmbedding(const QVariantHash &modelInfo);
private:
    ModelhubProviderPrivate *d = nullptr;
};

AIDAEMON_END_NAMESPACE

#endif // MODELHUBPROVIDER_H
