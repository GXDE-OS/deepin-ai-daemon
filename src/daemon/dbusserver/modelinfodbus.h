// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELINFODBUS_H
#define MODELINFODBUS_H

#include "aidaemon_global.h"

#include <QObject>
#include <QDBusContext>
#include <QJsonObject>

AIDAEMON_BEGIN_NAMESPACE

class ModelCenter;
class ModelProvidersConfig;

class ModelInfoDBus : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.ModelInfo")

public:
    explicit ModelInfoDBus(ModelCenter* modelCenter, QObject *parent = nullptr);
    ~ModelInfoDBus();

public Q_SLOTS:
    /**
     * Get list of supported AI capabilities
     * 
     * @return JSON string containing array of supported capabilities
     * Format: ["Chat", "SpeechToText", "TextToSpeech", "OCR", "ImageGeneration"]
     * 
     * Example return:
     * ["Chat", "FunctionCalling"]
     */
    QString GetSupportedCapabilities();

    /**
     * @brief Get all available models
     * @return JSON string containing all model information
     * Format: {
     *   "models": [
     *     {
     *       "name": "model_name",
     *       "provider": "provider_name", 
     *       "description": "model description",
     *       "capability": "TextChat",
     *       "deployType": "Local|Cloud|Custom",
     *       "isAvailable": true,
     *       "parameters": { ... }
     *     }
     *   ]
     * }
     */
    QString GetAllModels();

    /**
     * @brief Get available models for a specific capability
     * 
     * @param capability The capability to filter by (e.g. "Chat")
     * @return JSON string containing array of model information
     * 
     * Each model object contains:
     * {
     *   "modelName": "model_name",
     *   "provider": "provider_name", 
     *   "capability": "Chat",
     *   "parameters": {...}
     * }
     */
    QString GetModelsForCapability(const QString &capability);

    /**
     * @brief Get detailed information for a specific model
     * 
     * @param modelName The name of the model
     * @return JSON string containing model information
     * Format:
     * {
     *   "modelName": "model_name",
     *   "provider": "provider_name",
     *   "capability": "Chat", 
     *   "parameters": {...}
     * }
     */
    QString GetModelInfo(const QString &modelName);

    /**
     * @brief Get the currently selected model for a specific capability
     * 
     * @param capability The capability to get the current model for
     * @return The name of the currently selected model
     */
    QString GetCurrentModelForCapability(const QString &capability);

    
    /**
     * @brief Get list of all available model providers
     * 
     * @return QStringList containing provider names
     */
    QStringList GetProviderList();
    
    /**
     * @brief Get list of models for a specific provider
     * 
     * @param provider The provider name
     * @return JSON string containing array of model information
     */
    QString GetModelsForProvider(const QString &provider);

private:
    ModelCenter* modelCenter = nullptr;
    
    // Helper method to build empty models JSON response
    QString buildEmptyModelsJson();
    
    // Helper method to build model object from configuration
    QJsonObject buildModelObject(const QString &provider, const QString &capability, 
                               const QString &modelName, const ModelProvidersConfig &config);
};

AIDAEMON_END_NAMESPACE

#endif // MODELINFODBUS_H