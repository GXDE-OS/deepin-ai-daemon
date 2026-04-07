// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OPENAIPROVIDER_H
#define OPENAIPROVIDER_H

#include "modelcenter/providers/modelprovider.h"

AIDAEMON_BEGIN_NAMESPACE
class ChatInterface;
class EmbeddingInterface;
class FunctionCallingInterface;
class OpenAIProviderPrivate;
class OpenAIProvider : public ModelProvider
{
    friend class OpenAIProviderPrivate;
public:
    static inline QString providerName() {
        return "openai";
    }
    static OpenAIProvider *create();

    OpenAIProvider();
    ~OpenAIProvider() override;
public:
    QString name() override;
    AbstractInterface *createInterface(InterfaceType type) override;
    AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) override;
protected:
    ChatInterface *createChat(const QVariantHash &modelInfo);
    EmbeddingInterface *createEmbedding(const QVariantHash &modelInfo);
    FunctionCallingInterface *createFunctionCalling(const QVariantHash &modelInfo);
private:
    OpenAIProviderPrivate *d = nullptr;
};

AIDAEMON_END_NAMESPACE

#endif // OPENAIPROVIDER_H
