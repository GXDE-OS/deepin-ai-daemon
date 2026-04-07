// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelhubprovider_p.h"
#include "modelcenter/modelutils.h"
#include "modelhubchat.h"
#include "modelhubfunctioncalling.h"
#include "modelhubembedding.h"
#include "config/modelprovidersconfig.h"

#include <QSettings>
#include <QVariant>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE
using namespace modelhub;

ModelhubProviderPrivate::ModelhubProviderPrivate(ModelhubProvider *parent)
    : q(parent)
{
}

QSharedPointer<ModelhubWrapper> ModelhubProviderPrivate::instance(const QString &name)
{
    QSharedPointer<ModelhubWrapper> ret;
    auto it = m_wrapperMap.find(name);
    if (it == m_wrapperMap.end()) {
        qCInfo(logModelCenter) << "Creating new ModelhubWrapper instance for model:" << name;
        ret.reset(new ModelhubWrapper(name));
        ret->setKeepLive(true);
        m_wrapperMap.insert(name, ret);
    } else {
        qCDebug(logModelCenter) << "Reusing existing ModelhubWrapper instance for model:" << name;
        ret = *it;
    }

    return ret;
}

ModelhubProvider *ModelhubProvider::create()
{
    return new ModelhubProvider;
}

ModelhubProvider::ModelhubProvider()
    : ModelProvider()
    , d(new ModelhubProviderPrivate(this))
{

}

ModelhubProvider::~ModelhubProvider()
{
    delete d;
    d = nullptr;
}

QString ModelhubProvider::name()
{
    return providerName();
}

AbstractInterface *ModelhubProvider::createInterface(InterfaceType type)
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

AbstractInterface *ModelhubProvider::createInterface(InterfaceType type, const QVariantHash &modelInfo)
{
    AbstractInterface *ret = nullptr;
    switch (type) {
    case Chat:
        ret = createChat(modelInfo);
        break;
    case FunctionCalling:
        ret = createFunctionCalling(modelInfo);
        break;
    case TextEmbedding:
        ret = createEmbedding(modelInfo);
        break;
    default:
        break;
    }

    return ret;
}

ChatInterface *ModelhubProvider::createChat(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    if (model.isEmpty()) {
        qCWarning(logModelCenter) << "No chat model configured for provider:" << name();
        return nullptr;
    }

    qCInfo(logModelCenter) << "Creating chat interface with model:" << model;
    auto chat = new ModelhubChat(model, d->instance(model));
    return chat;
}

FunctionCallingInterface *ModelhubProvider::createFunctionCalling(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    if (model.isEmpty()) {
        qCWarning(logModelCenter) << "No function calling model configured for provider:" << name();
        return nullptr;
    }

    qCInfo(logModelCenter) << "Creating function calling interface with model:" << model;
    QSharedPointer<ModelhubChat> chat(new ModelhubChat(model, d->instance(model)));

    auto func = new ModelhubFunctionCalling(chat);
    return func;
}

EmbeddingInterface *ModelhubProvider::createEmbedding(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    if (model.isEmpty()) {
        qCWarning(logModelCenter) << "No embedding model configured for provider:" << name();
        return nullptr;
    }

    qCInfo(logModelCenter) << "Creating embedding interface with model:" << model;
    auto emb = new ModelhubEmbedding(model, d->instance(model));
    return emb;
}


