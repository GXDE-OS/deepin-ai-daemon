// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ernieprovider_p.h"
#include "modelcenter/modelutils.h"
#include "erniechat.h"
#include "erniefunctioncalling.h"
#include "config/modelprovidersconfig.h"

#include <QSettings>
#include <QVariant>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

ErnieProviderPrivate::ErnieProviderPrivate(ErnieProvider *parent) : q(parent)
{

}

ErnieProvider::ErnieProvider()
    : ModelProvider()
    , d(new ErnieProviderPrivate(this))
{

}

ErnieProvider::~ErnieProvider()
{
    delete d;
    d = nullptr;
}

QString ErnieProvider::name()
{
    return providerName();
}

AbstractInterface *ErnieProvider::createInterface(InterfaceType type)
{
    QVariantHash modelInfo;
    ModelProvidersConfig cfg;
    switch (type) {
    case Chat:
        modelInfo = cfg.getProviderConfig(name(), "Chat");
        break;
    case FunctionCalling:
        modelInfo = cfg.getProviderConfig(name(), "FunctionCalling");
        break;
    default:
        break;
    }

    return createInterface(type, modelInfo);
}

AbstractInterface *ErnieProvider::createInterface(InterfaceType type, const QVariantHash &modelInfo)
{
    AbstractInterface *ret = nullptr;
    switch (type) {
    case Chat:
        ret = createChat(modelInfo);
        break;
    case FunctionCalling:
        ret = createFunctionCalling(modelInfo);
        break;
    default:
        break;
    }

    return ret;
}

ChatInterface *ErnieProvider::createChat(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel, QString("ERNIE-Lite-8K")).toString();
    QString apikey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiSecret = modelInfo.value(kModelInfoApiSecret).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();

    qCInfo(logModelCenter) << "Creating new ErnieChat instance with model:" << model << "and apiUrl:" << apiUrl;
    auto chat = new ernie::ErnieChat(model);

    chat->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating Ernie chat interface";

    chat->setApiSecret(apiSecret);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API Secret when creating Ernie chat interface";

    chat->setApiPath(apiUrl);
    if (apiUrl.isEmpty())
        qCWarning(logModelCenter) << "Empty API URL when creating Ernie chat interface";

    return chat;
}

FunctionCallingInterface *ErnieProvider::createFunctionCalling(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel, QString("ERNIE-Lite-8K")).toString();
    QString apikey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiSecret = modelInfo.value(kModelInfoApiSecret).toString();
    QString apiUrl = modelInfo.value(kModelInfoApiUrl).toString();
    qCInfo(logModelCenter) << "Creating new ErnieFunctionCalling instance with model:" << model << "and apiUrl:" << apiUrl;

    QSharedPointer<ernie::ErnieChat> chat(new ernie::ErnieChat(model));
    chat->setApiKey(apikey);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API key when creating Ernie chat interface";

    chat->setApiSecret(apiSecret);
    if (apikey.isEmpty())
        qCWarning(logModelCenter) << "Empty API Secret when creating Ernie chat interface";

    chat->setApiPath(apiUrl);
    if (apiUrl.isEmpty())
        qCWarning(logModelCenter) << "Empty API URL when creating Ernie chat interface";

    auto func = new ernie::ErnieFunctionCalling(chat);
    return func;
}

ErnieProvider *ErnieProvider::create()
{
    return new ErnieProvider;
}

