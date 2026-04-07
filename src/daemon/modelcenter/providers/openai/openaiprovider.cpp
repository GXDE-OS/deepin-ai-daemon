// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "openaiprovider_p.h"
#include "modelcenter/modelutils.h"
#include "universalchatinterface.h"
#include "universalembeddinginterface.h"
#include "universalfunctioncallinginterface.h"
#include "config/modelprovidersconfig.h"

#include <QVariant>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

OpenAIProviderPrivate::OpenAIProviderPrivate(OpenAIProvider *parent)
    : q(parent)
{

}

OpenAIProvider *OpenAIProvider::create()
{
    return new OpenAIProvider;
}

OpenAIProvider::OpenAIProvider()
    : d(new OpenAIProviderPrivate(this))
{

}

OpenAIProvider::~OpenAIProvider()
{
    delete d;
    d = nullptr;
}

QString OpenAIProvider::name()
{
    return providerName();
}

AbstractInterface *OpenAIProvider::createInterface(InterfaceType type)
{
    QVariantHash modelInfo;
    ModelProvidersConfig cfg;
    switch (type) {
    case Chat:
        modelInfo = cfg.getProviderConfig(name(), "Chat");
        break;
    case TextEmbedding:
        modelInfo = cfg.getProviderConfig(name(), "Embedding");
        break;
    case FunctionCalling:
        modelInfo = cfg.getProviderConfig(name(), "FunctionCalling");
        break;
    default:
        break;
    }

    return createInterface(type, modelInfo);
}

AbstractInterface *OpenAIProvider::createInterface(InterfaceType type, const QVariantHash &modelInfo)
{
    AbstractInterface *ret = nullptr;
    switch (type) {
    case Chat:
        ret = createChat(modelInfo);
        break;
    case TextEmbedding:
        ret = createEmbedding(modelInfo);
        break;
    case FunctionCalling:
        ret = createFunctionCalling(modelInfo);
        break;
    default:
        break;
    }

    return ret;
}

ChatInterface *OpenAIProvider::createChat(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    QString apikey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();

    auto chat = new UniversalChatInterface(model);
    if (model.isEmpty())
        qCWarning(logModelCenter) << "Empty model when creating OpenAI chat interface";

    chat->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating OpenAI chat interface";

    chat->setApiUrl(apiUrl);
    if (apiUrl.isEmpty())
        qCWarning(logModelCenter) << "Empty API URL when creating OpenAI chat interface";

    return chat;
}

EmbeddingInterface *OpenAIProvider::createEmbedding(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    QString apikey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();

    auto emb = new UniversalEmbeddingInterface(model);
    if (model.isEmpty())
        qCWarning(logModelCenter) << "Empty model when creating OpenAI embedding interface";

    emb->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating OpenAI embedding interface";

    emb->setApiUrl(apiUrl);
    if (apiUrl.isEmpty())
        qCWarning(logModelCenter) << "Empty API URL when creating OpenAI embedding interface";

    return emb;
}

FunctionCallingInterface *OpenAIProvider::createFunctionCalling(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    QString apikey = modelInfo.value(kModelInfoApiUrl).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiKey).toString();

    QSharedPointer<UniversalChatInterface> chat(new UniversalChatInterface(model));
    if (model.isEmpty())
        qCWarning(logModelCenter) << "Empty model when creating OpenAI chat interface";

    chat->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating OpenAI chat interface";

    chat->setApiUrl(apiUrl);
    if (apiUrl.isEmpty())
        qCWarning(logModelCenter) << "Empty API URL when creating OpenAI chat interface";

    auto func = new UniversalFunctionCallingInterface(chat);
    return func;
}
