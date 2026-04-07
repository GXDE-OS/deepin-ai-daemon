// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "moddelfactory.h"

#include "providers/modelhub/modelhubprovider.h"
#include "providers/doubao/doubaoprovider.h"
#include "providers/baidu/ernieprovider.h"
#include "providers/xunfei/xunfeiprovider.h"
#include "providers/dtk/dtkprovider.h"
#include "providers/openai/openaiprovider.h"
#include "modelutils.h"
#include "config/modelprovidersconfig.h"

#include <QSettings>
#include <QVariant>
#include <QLoggingCategory>

AIDAEMON_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

ModdelFactory::ModdelFactory(QObject *parent) : QObject(parent)
{
    providers.insert(ModelhubProvider::providerName(), ModelhubProvider::create);
    providers.insert(OpenAIProvider::providerName(), OpenAIProvider::create);
    providers.insert(DouBaoProvider::providerName(), DouBaoProvider::create);
    providers.insert(ErnieProvider::providerName(), ErnieProvider::create);
    providers.insert(XunfeiProvider::providerName(), XunfeiProvider::create);
    providers.insert(DTKProvider::providerName(), DTKProvider::create);
}

QSharedPointer<ModelProvider> ModdelFactory::create(const QString &name)
{
    auto it = providers.find(name);
    if (it == providers.end()) {
        qCWarning(logModelCenter) << "Requested provider not found:" << name;
        return nullptr;
    }

    qCDebug(logModelCenter) << "Creating provider:" << name;
    return QSharedPointer<ModelProvider>((*it)());
}

QSharedPointer<ModelProvider> ModdelFactory::createDefault(InterfaceType type)
{
    QSharedPointer<ModelProvider> ret;
    
    // Use ModelProvidersConfig to read JSON configuration
    ModelProvidersConfig config;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Model providers configuration is invalid";
        return nullptr;
    }
    
    // Convert InterfaceType to ServiceType string
    ServiceType serviceType = interfaceTypeToServiceType(type);
    QString interfaceTypeStr = AITypes::serviceTypeToString(serviceType);
    
    // Get default provider from configuration
    QString defaultProvider = config.getDefaultProvider(interfaceTypeStr);
    
    if (defaultProvider.isEmpty()) {
        qCWarning(logModelCenter) << "No default provider configured for interface type:" << interfaceTypeStr;
        return nullptr;
    }

    // Create the provider
    ret = create(defaultProvider);
    qCInfo(logModelCenter) << "Created default" << interfaceTypeStr << "provider:" 
                          << (ret ? ret->name() : "null");
    
    return ret;
}
