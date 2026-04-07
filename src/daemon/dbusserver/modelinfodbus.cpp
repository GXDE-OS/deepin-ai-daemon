// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelinfodbus.h"
#include "modelcenter/modelcenter.h"
#include "config/modelprovidersconfig.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QSet>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

ModelInfoDBus::ModelInfoDBus(ModelCenter* modelCenter, QObject *parent)
    : QObject(parent)
    , QDBusContext()
    , modelCenter(modelCenter)
{
    qCDebug(logModelCenter) << "ModelInfoDBus initialized";
}

ModelInfoDBus::~ModelInfoDBus()
{
    qCDebug(logModelCenter) << "ModelInfoDBus destroyed";
}

QString ModelInfoDBus::GetSupportedCapabilities()
{
    qCDebug(logModelCenter) << "GetSupportedCapabilities called";
    
    // Get capabilities from configuration
    ModelProvidersConfig config;
    QJsonArray capabilities;
    
    if (config.isValid()) {
        // Get all supported interface types from configuration
        QStringList providers = config.getAvailableProviders();
        QSet<QString> supportedInterfaces;
        
        for (const QString &provider : providers) {
            // Get all interface types supported by this provider
            QStringList interfaceTypes = config.getSupportedInterfaces(provider);
            for (const QString &interfaceType : interfaceTypes) {
                supportedInterfaces.insert(interfaceType);
            }
        }
        
        // Convert to JSON array
        for (const QString &interface : supportedInterfaces) {
            capabilities.append(interface);
        }
    }
    
    QJsonDocument doc(capabilities);
    QString result = doc.toJson(QJsonDocument::Compact);
    
    qCInfo(logModelCenter) << "Returned supported capabilities:" << capabilities.size() << "items";
    return result;
}

QString ModelInfoDBus::GetAllModels()
{
    qCDebug(logModelCenter) << "GetAllModels called";
    
    if (!modelCenter) {
        qCWarning(logModelCenter) << "ModelCenter is null, returning empty model list";
        return buildEmptyModelsJson();
    }
    
    ModelProvidersConfig config;
    QJsonObject response;
    QJsonArray modelsArray;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Invalid configuration, returning empty model list";
        return buildEmptyModelsJson();
    }
    
    QStringList providers = config.getAvailableProviders();
    
    for (const QString &provider : providers) {
        // Get all interface types supported by this provider
        QStringList interfaceTypes = config.getSupportedInterfaces(provider);
        
        for (const QString &interfaceType : interfaceTypes) {
            QString model = config.getModelName(provider, interfaceType);
            if (!model.isEmpty()) {
                QJsonObject modelObj = buildModelObject(provider, interfaceType, model, config);
                modelsArray.append(modelObj);
            }
        }
    }
    
    response["models"] = modelsArray;
    
    QJsonDocument doc(response);
    QString result = doc.toJson(QJsonDocument::Compact);
    
    qCInfo(logModelCenter) << "Returned all models:" << modelsArray.size() << "models";
    return result;
}

QString ModelInfoDBus::GetModelsForCapability(const QString &capability)
{
    qCDebug(logModelCenter) << "GetModelsForCapability called with capability:" << capability;
    
    if (capability.isEmpty()) {
        qCWarning(logModelCenter) << "Empty capability provided";
        return buildEmptyModelsJson();
    }
    
    ModelProvidersConfig config;
    QJsonObject response;
    QJsonArray modelsArray;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Invalid configuration";
        return buildEmptyModelsJson();
    }
    
    QStringList providers = config.getAvailableProviders();
    
    for (const QString &provider : providers) {
        // Check if provider supports the requested capability (interface type)
        if (config.supportsInterface(provider, capability)) {
            QString model = config.getModelName(provider, capability);
            if (!model.isEmpty()) {
                QJsonObject modelObj = buildModelObject(provider, capability, model, config);
                modelsArray.append(modelObj);
            }
        }
    }
    
    response["models"] = modelsArray;
    
    QJsonDocument doc(response);
    QString result = doc.toJson(QJsonDocument::Compact);
    
    qCInfo(logModelCenter) << "Returned models for capability" << capability << ":" 
                          << modelsArray.size() << "models";
    return result;
}

QString ModelInfoDBus::GetModelInfo(const QString &modelName)
{
    qCDebug(logModelCenter) << "GetModelInfo called with model:" << modelName;
    
    if (modelName.isEmpty()) {
        qCWarning(logModelCenter) << "Empty model name provided";
        return "{}";
    }
    
    // Search for the model in all providers
    ModelProvidersConfig config;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Invalid configuration";
        return "{}";
    }
    
    QStringList providers = config.getAvailableProviders();
    
    for (const QString &provider : providers) {
        QStringList models = config.getProviderModels(provider);
        
        if (models.contains(modelName)) {
            // Found the model, now find which interface type(s) it supports
            QStringList interfaceTypes = config.getSupportedInterfaces(provider);
            
            for (const QString &interfaceType : interfaceTypes) {
                if (config.getModelName(provider, interfaceType) == modelName) {
                    QJsonObject modelObj = buildModelObject(provider, interfaceType, modelName, config);
                    QJsonDocument doc(modelObj);
                    QString result = doc.toJson(QJsonDocument::Compact);
                    
                    qCInfo(logModelCenter) << "Found model info for:" << modelName 
                                          << "with capability:" << interfaceType;
                    return result;
                }
            }
        }
    }
    
    qCWarning(logModelCenter) << "Model not found:" << modelName;
    return "{}";
}

QString ModelInfoDBus::GetCurrentModelForCapability(const QString &capability)
{
    qCDebug(logModelCenter) << "GetCurrentModelForCapability called with capability:" << capability;
    
    if (capability.isEmpty()) {
        qCWarning(logModelCenter) << "Empty capability provided";
        return QString();
    }
    
    ModelProvidersConfig config;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Invalid configuration";
        return QString();
    }
    
    // Get the default provider for this capability
    QString provider = config.getDefaultProvider(capability);
    if (provider.isEmpty()) {
        qCWarning(logModelCenter) << "No default provider found for capability:" << capability;
        return QString();
    }
    
    // Get the model name for this provider and capability
    QString modelName = config.getModelName(provider, capability);
    qCInfo(logModelCenter) << "Current model for capability" << capability 
                          << "is" << modelName << "from provider" << provider;
    return modelName;
}


QStringList ModelInfoDBus::GetProviderList()
{
    qCDebug(logModelCenter) << "GetProviderList called";
    
    ModelProvidersConfig config;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Invalid configuration";
        return QStringList();
    }
    
    QStringList providers = config.getAvailableProviders();
    
    qCInfo(logModelCenter) << "Returned provider list with" << providers.size() << "providers";
    return providers;
}

QString ModelInfoDBus::GetModelsForProvider(const QString &provider)
{
    qCDebug(logModelCenter) << "GetModelsForProvider called with provider:" << provider;
    
    if (provider.isEmpty()) {
        qCWarning(logModelCenter) << "Empty provider provided";
        return buildEmptyModelsJson();
    }
    
    ModelProvidersConfig config;
    QJsonObject response;
    QJsonArray modelsArray;
    
    if (!config.isValid()) {
        qCWarning(logModelCenter) << "Invalid configuration";
        return buildEmptyModelsJson();
    }
    
    // Get all models for this provider
    QStringList models = config.getProviderModels(provider);
    
    // For each model, get its capabilities and build model objects
    for (const QString &model : models) {
        // Get all interface types supported by this provider
        QStringList interfaceTypes = config.getSupportedInterfaces(provider);
        
        for (const QString &interfaceType : interfaceTypes) {
            // Check if this model is used for this interface type
            if (config.getModelName(provider, interfaceType) == model) {
                QJsonObject modelObj = buildModelObject(provider, interfaceType, model, config);
                modelsArray.append(modelObj);
            }
        }
    }
    
    response["models"] = modelsArray;
    
    QJsonDocument doc(response);
    QString result = doc.toJson(QJsonDocument::Compact);
    
    qCInfo(logModelCenter) << "Returned models for provider" << provider << ":" 
                          << modelsArray.size() << "models";
    return result;
}

QString ModelInfoDBus::buildEmptyModelsJson()
{
    QJsonObject response;
    response["models"] = QJsonArray();
    
    QJsonDocument doc(response);
    return doc.toJson(QJsonDocument::Compact);
}

QJsonObject ModelInfoDBus::buildModelObject(const QString &provider, const QString &capability, 
                                           const QString &modelName, const ModelProvidersConfig &config)
{
    QJsonObject modelObj;
    modelObj["name"] = modelName;
    modelObj["provider"] = provider;
    modelObj["capability"] = capability;
    
    // Set deploy type based on provider
    if (provider == "modelhub") {
        modelObj["deployType"] = "Local";
        modelObj["description"] = QString("%1 local model for %2").arg(modelName, capability);
        modelObj["isAvailable"] = true; // Assume local models are available
    } else {
        modelObj["deployType"] = "Cloud";
        modelObj["description"] = QString("%1 cloud model for %2").arg(modelName, capability);
        modelObj["isAvailable"] = true; // Assume cloud models are available if configured
    }
    
    // Get provider configuration using the capability as interface type directly
    QVariantHash providerConfig = config.getProviderConfig(provider, capability);
    
    // Build parameters from configuration
    QJsonObject parameters;
    for (auto it = providerConfig.begin(); it != providerConfig.end(); ++it) {
        if (it.key() != "Model") { // Exclude the model name itself
            parameters[it.key()] = QJsonValue::fromVariant(it.value());
        }
    }
    
    modelObj["parameters"] = parameters;
    
    return modelObj;
}