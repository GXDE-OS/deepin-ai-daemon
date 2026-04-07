// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doubaoprovider_p.h"
#include "modelcenter/modelutils.h"
#include "doubaochat.h"
#include "doubaovision.h"
#include "doubaoembedding.h"
#include "config/modelprovidersconfig.h"

#include <QSettings>
#include <QVariant>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

DouBaoProviderPrivate::DouBaoProviderPrivate(DouBaoProvider *parent)
    : q(parent)
{

}

DouBaoProvider *DouBaoProvider::create()
{
    return new DouBaoProvider;
}

DouBaoProvider::DouBaoProvider()
    : d(new DouBaoProviderPrivate(this))
{

}

DouBaoProvider::~DouBaoProvider()
{
    delete d;
    d = nullptr;
}

QString DouBaoProvider::name()
{
    return providerName();
}

AbstractInterface *DouBaoProvider::createInterface(InterfaceType type)
{
    QVariantHash modelInfo;
    ModelProvidersConfig cfg;
    switch (type) {
    case Chat:
        modelInfo = cfg.getProviderConfig(name(), "Chat");
        break;
    case ImageRecognition:
        modelInfo = cfg.getProviderConfig(name(), "ImageRecognition");
        break;
    case TextEmbedding:
        modelInfo = cfg.getProviderConfig(name(), "FunctionCalling");
        break;
    default:
        break;
    }

    return createInterface(type, modelInfo);
}

AbstractInterface *DouBaoProvider::createInterface(InterfaceType type, const QVariantHash &modelInfo)
{
    AbstractInterface *ret = nullptr;
    switch (type) {
    case Chat:
        ret = createChat(modelInfo);
        break;
    case ImageRecognition:
        ret = createImageRecognition(modelInfo);
        break;
    case TextEmbedding:
        ret = createEmbedding(modelInfo);
        break;
    default:
        break;
    }

    return ret;
}

ChatInterface *DouBaoProvider::createChat(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    QString apikey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();

    auto chat = new doubao::DouBaoChat(model);
    if (model.isEmpty())
        qCWarning(logModelCenter) << "Empty model when creating DouBao chat interface";

    chat->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating DouBao chat interface";

    if (!apiUrl.isEmpty()) {
        qCInfo(logModelCenter) << "Setting custom API URL:" << apiUrl;
        chat->setApiUrl(apiUrl);
    }

    return chat;
}

ImageRecognitionInterface *DouBaoProvider::createImageRecognition(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    QString apiKey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();
    int maxTokens = modelInfo.value(kModelInfoMaxTokens, 4096).toInt();
    double temperature = modelInfo.value("Temperature", 0.8).toDouble();

    auto vision = new doubao::DouBaoVision(model);
    if (model.isEmpty())
        qCWarning(logModelCenter) << "Empty model when creating DouBao vision interface";
    vision->setApiKey(apiKey);

    if (apiKey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating DouBao vision interface";

    vision->setApiUrl(apiUrl);
    if (apiUrl.isEmpty())
        qCWarning(logModelCenter) << "Empty API url when creating DouBao vision interface";

    vision->setMaxTokens(maxTokens);
    vision->setTemperature(temperature);
    
    qCInfo(logModelCenter) << "Creating DouBao image recognition interface with model:" << model;
    return vision;
}

EmbeddingInterface *DouBaoProvider::createEmbedding(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel).toString();
    QString apikey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();

    auto emb = new doubao::DouBaoEmbedding(model);
    if (model.isEmpty())
        qCWarning(logModelCenter) << "Empty model when creating DouBao embedding interface";

    emb->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating DouBao embedding interface";

    if (!apiUrl.isEmpty()) {
        qCInfo(logModelCenter) << "Setting custom API URL:" << apiUrl;
        emb->setApiUrl(apiUrl);
    }

    return emb;
}
