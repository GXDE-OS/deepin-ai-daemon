// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "configmanager.h"
#include "logcategory.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
}

bool ConfigManager::loadConfig(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(logConfigManager) << "Failed to open config file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qCWarning(logConfigManager) << "Failed to parse JSON from config file:" << filePath;
        return false;
    }

    rootObject = doc.object();
    parseConfig();
    return true;
}

bool ConfigManager::saveConfig(const QString &filePath)
{
    // Update rootObject from models, defaultProviders, and providerNames
    QJsonObject defaultsObject;
    for (auto it = defaultProviders.constBegin(); it != defaultProviders.constEnd(); ++it) {
        defaultsObject[it.key()] = it.value();
    }
    rootObject["defaults"] = defaultsObject;

    QJsonObject providersObject;
    QSet<QString> allProviders;
    for (const auto &configList : models) {
        for (const auto &config : configList) {
            allProviders.insert(config.provider);
        }
    }

    for (const QString &providerId : allProviders) {
        QJsonObject providerNode;
        providerNode["name"] = providerNames.value(providerId);

        QJsonObject modelsObject;
        bool providerHasModels = false;
        for (auto typeIt = models.constBegin(); typeIt != models.constEnd(); ++typeIt) {
            const QString &modelType = typeIt.key();
            for (const auto &config : typeIt.value()) {
                if (config.provider == providerId) {
                    // Find the model name for this type and provider
                    for(auto paramIt = config.parameters.constBegin(); paramIt != config.parameters.constEnd(); ++paramIt) {
                        if (paramIt.key() == "model") {
                             modelsObject[modelType] = paramIt.value();
                             providerHasModels = true;
                        } else if (paramIt.key() != "name") {
                            providerNode[paramIt.key()] = paramIt.value();
                        }
                    }
                }
            }
        }
        if(providerHasModels)
            providerNode["models"] = modelsObject;

        providersObject[providerId] = providerNode;
    }
    rootObject["providers"] = providersObject;


    QJsonDocument doc(rootObject);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(logConfigManager) << "Failed to open config file for writing:" << filePath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

void ConfigManager::parseConfig()
{
    models.clear();
    defaultProviders.clear();
    providerNames.clear();

    qCDebug(logConfigManager) << "=== ConfigManager::parseConfig() ===";

    // Parse defaults
    if (rootObject.contains("defaults") && rootObject["defaults"].isObject()) {
        QJsonObject defaults = rootObject["defaults"].toObject();
        for (const QString &key : defaults.keys()) {
            defaultProviders[key] = defaults[key].toString();
            qCDebug(logConfigManager) << "Default provider:" << key << "=" << defaults[key].toString();
        }
    }

    // Parse providers
    if (rootObject.contains("providers") && rootObject["providers"].isObject()) {
        QJsonObject providers = rootObject["providers"].toObject();
        for (const QString &providerId : providers.keys()) {
            QJsonObject providerData = providers[providerId].toObject();
            
            if (providerData.contains("name") && providerData["name"].isString()) {
                providerNames[providerId] = providerData["name"].toString();
                qCDebug(logConfigManager) << "Provider name:" << providerId << "=" << providerData["name"].toString();
            }

            if (providerData.contains("models") && providerData["models"].isObject()) {
                QJsonObject providerModels = providerData["models"].toObject();
                for (const QString &modelType : providerModels.keys()) {
                    ModelConfig config;
                    config.provider = providerId;
                    config.modelType = modelType;
                    config.parameters["model"] = providerModels[modelType].toString();

                    // Add other provider-level parameters
                    for (const QString &paramKey : providerData.keys()) {
                        if (paramKey != "name" && paramKey != "models") {
                            config.parameters[paramKey] = providerData[paramKey].toVariant().toString();
                        }
                    }
                    
                    config.enabled = (defaultProviders.value(modelType) == providerId);
                    models[modelType].append(config);
                    qCDebug(logConfigManager) << "Added model:" << providerId << "for type" << modelType << "enabled:" << config.enabled;
                }
            }
        }
    }

    qCDebug(logConfigManager) << "Final model counts:";
    for (auto it = models.begin(); it != models.end(); ++it) {
        qCDebug(logConfigManager) << "  " << it.key() << ":" << it.value().size() << "models";
    }
}

QStringList ConfigManager::getModelTypes() const
{
    QStringList types;
    types << "Chat" << "ImageRecognition" << "SpeechToText" << "TextToSpeech" << "FunctionCalling";
    return types;
}

QStringList ConfigManager::getProvidersForType(const QString &modelType) const
{
    QStringList providers;
    if (models.contains(modelType)) {
        for (const auto &config : models[modelType]) {
            if (!providers.contains(config.provider)) {
                providers.append(config.provider);
            }
        }
    }
    
    providers.sort();
    
    if (providers.isEmpty()) {
        providers = getDefaultProvidersForType(modelType);
    }
    
    return providers;
}

QList<ModelConfig> ConfigManager::getModelsForType(const QString &modelType) const
{
    return models.value(modelType);
}

void ConfigManager::addModel(const ModelConfig &config)
{
    // Validate that Model field is present for all configurations
    ModelConfig validatedConfig = config;
    if (!validatedConfig.parameters.contains("Model")) {
        // Generate a default model name if missing
        QString defaultModelName;
        if (config.provider == "xunfei") {
            if (config.modelType == "SpeechToText") {
                defaultModelName = "iat";
            } else if (config.modelType == "TextToSpeech") {
                defaultModelName = "tts";
            } else if (config.modelType == "OCRRecognition") {
                defaultModelName = "ocr";
            }
        } else {
            // For other providers, use a generic pattern
            defaultModelName = QString("%1-%2-default").arg(config.provider.toLower()).arg(config.modelType.toLower());
        }
        
        validatedConfig.parameters["Model"] = defaultModelName;
        qCWarning(logConfigManager) << "Added missing Model field for" << config.provider << config.modelType 
                                   << "with default value:" << defaultModelName;
    }
    
    models[validatedConfig.modelType].append(validatedConfig);
    
    // If this is the first model of this type, set it as default
    if (!defaultProviders.contains(validatedConfig.modelType)) {
        defaultProviders[validatedConfig.modelType] = validatedConfig.provider;
        qCDebug(logConfigManager) << "Set" << validatedConfig.provider << "as default provider for" << validatedConfig.modelType;
    }
    
    // Update enabled status for all models of this type
    updateModelEnabledStatus(validatedConfig.modelType);
}

void ConfigManager::updateModel(int index, const ModelConfig &config)
{
    if (models.contains(config.modelType) && index >= 0 && index < models[config.modelType].size()) {
        // Apply the same validation as addModel
        ModelConfig validatedConfig = config;
        if (!validatedConfig.parameters.contains("Model")) {
            // Generate a default model name if missing
            QString defaultModelName;
            if (config.provider == "xunfei") {
                if (config.modelType == "SpeechToText") {
                    defaultModelName = "iat";
                } else if (config.modelType == "TextToSpeech") {
                    defaultModelName = "tts";
                } else if (config.modelType == "OCRRecognition") {
                    defaultModelName = "ocr";
                }
            } else {
                // For other providers, use a generic pattern
                defaultModelName = QString("%1-%2-default").arg(config.provider.toLower()).arg(config.modelType.toLower());
            }
            
            validatedConfig.parameters["Model"] = defaultModelName;
            qCWarning(logConfigManager) << "Added missing Model field during update for" << config.provider << config.modelType 
                                       << "with default value:" << defaultModelName;
        }
        
        models[config.modelType][index] = validatedConfig;
    }
}

void ConfigManager::removeModel(const QString &modelType, int index)
{
    if (models.contains(modelType) && index >= 0 && index < models[modelType].size()) {
        // Get the provider of the model to be removed
        QString providerToRemove = models[modelType][index].provider;
        
        // Remove the model from the list
        models[modelType].removeAt(index);
        
        // Check if this provider was the default for this model type
        QString currentDefault = defaultProviders.value(modelType);
        if (currentDefault == providerToRemove) {
            // Check if there are other models of the same type with the same provider
            bool providerStillExists = false;
            for (const auto &config : models[modelType]) {
                if (config.provider == providerToRemove) {
                    providerStillExists = true;
                    break;
                }
            }
            
            if (!providerStillExists) {
                // Remove from default providers since no models of this provider exist for this type
                defaultProviders.remove(modelType);
                qCDebug(logConfigManager) << "Removed default provider" << providerToRemove << "for" << modelType << "as no models remain";
                
                // If there are other models of this type, set the first one as default
                if (!models[modelType].isEmpty()) {
                    QString newDefault = models[modelType].first().provider;
                    defaultProviders[modelType] = newDefault;
                    qCDebug(logConfigManager) << "Set new default provider" << newDefault << "for" << modelType;
                    
                    // Update enabled status
                    updateModelEnabledStatus(modelType);
                }
            }
        }
        
        qCDebug(logConfigManager) << "Removed model at index" << index << "for type" << modelType;
    }
}

QString ConfigManager::getProviderName(const QString &provider) const
{
    // First try to get from loaded configuration
    if (providerNames.contains(provider)) {
        return providerNames.value(provider);
    }
    
    // If not found, return default friendly names
    if (provider == "doubao") {
        return "豆包";
    } else if (provider == "ernie") {
        return "百度";
    } else if (provider == "modelhub") {
        return "本地";
    } else if (provider == "xunfei") {
        return "讯飞";
    }
    
    // Fallback to provider ID
    return provider;
}

QString ConfigManager::getDefaultProvider(const QString &modelType) const
{
    return defaultProviders.value(modelType);
}

void ConfigManager::setDefaultProvider(const QString &modelType, const QString &provider)
{
    defaultProviders[modelType] = provider;
}

void ConfigManager::updateModelEnabledStatus(const QString &modelType)
{
    if (models.contains(modelType)) {
        QString defaultProvider = defaultProviders.value(modelType);
        
        // Update enabled status for all models of this type
        for (int i = 0; i < models[modelType].size(); ++i) {
            models[modelType][i].enabled = (models[modelType][i].provider == defaultProvider);
        }
        
        qCDebug(logConfigManager) << "Updated enabled status for" << modelType << "default provider:" << defaultProvider;
    }
}

QStringList ConfigManager::getProviderParameters(const QString &provider, const QString &modelType) const
{
    return getKnownParameters(provider, modelType);
}

QStringList ConfigManager::getKnownParameters(const QString &provider, const QString &modelType) const
{
    QStringList params;
    QSet<QString> paramSet;

    if (rootObject.contains("providers") && rootObject["providers"].isObject()) {
        QJsonObject providers = rootObject["providers"].toObject();
        if (providers.contains(provider)) {
            QJsonObject providerObj = providers[provider].toObject();
            for (const QString &key : providerObj.keys()) {
                if (key != "name" && key != "models") {
                    paramSet.insert(key);
                }
            }
        }
    }

    // Also add the "model" parameter which is inside the "models" object
    paramSet.insert("model");

    params = paramSet.values();
    
    // If no parameters found in config file, use default parameters
    if (params.isEmpty()) {
        params = getDefaultParameters(provider, modelType);
        qCDebug(logConfigManager) << "Using default parameters for" << provider << modelType << ":" << params;
    } else {
        qCDebug(logConfigManager) << "Found existing parameters for" << provider << modelType << ":" << params;
    }
    
    return params;
}

QStringList ConfigManager::getDefaultParameters(const QString &provider, const QString &modelType) const
{
    QStringList params;
    
    // Define default parameter templates based on current configuration patterns
    if (provider == "doubao") {
        if (modelType == "Chat") {
            params << "ApiKey" << "ApiUrl" << "Model";
        } else if (modelType == "ImageRecognition") {
            params << "ApiKey" << "ApiUrl" << "Model";
        }
    } else if (provider == "ernie") {
        if (modelType == "FunctionCalling") {
            params << "Model";
        } else if (modelType == "Chat") {
            params << "ApiKey" << "Model";
        }
    } else if (provider == "modelhub") {
        if (modelType == "Chat") {
            params << "Model";
        } else if (modelType == "FunctionCalling") {
            params << "Model";
        } else if (modelType == "ImageRecognition") {
            params << "Model";
        }
    } else if (provider == "xunfei") {
        if (modelType == "SpeechToText") {
            params << "ApiKey" << "ApiSecret" << "AppId" << "Model";
        } else if (modelType == "TextToSpeech") {
            params << "ApiKey" << "ApiSecret" << "AppId" << "Model";
        } else if (modelType == "OCRRecognition") {
            params << "ApiKey" << "ApiSecret" << "AppId" << "Model";
        }
    }
    
    // If no specific template found, provide common parameters
    if (params.isEmpty()) {
        if (modelType == "Chat" || modelType == "ImageRecognition") {
            params << "ApiKey" << "ApiUrl" << "Model";
        } else if (modelType == "SpeechToText" || modelType == "TextToSpeech" || modelType == "OCRRecognition") {
            params << "ApiKey" << "ApiSecret" << "AppId" << "Model";
        } else if (modelType == "FunctionCalling") {
            params << "Model";
        } else {
            // Generic fallback - always include Model field
            params << "ApiKey" << "Model";
        }
    }
    
    params.sort();
    return params;
}

QStringList ConfigManager::getDefaultProvidersForType(const QString &modelType) const
{
    QStringList providers;
    
    // Define which providers support which model types based on current configuration
    if (modelType == "Chat") {
        providers << "doubao" << "modelhub";
    } else if (modelType == "ImageRecognition") {
        providers << "doubao" << "modelhub";
    } else if (modelType == "SpeechToText") {
        providers << "xunfei";
    } else if (modelType == "TextToSpeech") {
        providers << "xunfei";
    } else if (modelType == "FunctionCalling") {
        providers << "ernie" << "modelhub";
    }
    
    // Always include all providers as potential options
    QStringList allProviders;
    allProviders << "doubao" << "ernie" << "modelhub" << "xunfei";
    
    // Merge specific providers with all providers (remove duplicates)
    for (const QString &provider : allProviders) {
        if (!providers.contains(provider)) {
            providers.append(provider);
        }
    }
    
    providers.sort();
    return providers;
}
