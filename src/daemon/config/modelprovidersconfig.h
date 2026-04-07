// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELPROVIDERSCONFIG_H
#define MODELPROVIDERSCONFIG_H

#include "aidaemon_global.h"

#include <QJsonObject>
#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

/**
 * @brief Simple wrapper for modelproviders.json configuration file
 *
 * This class provides a simple interface to query model provider configurations.
 * The configuration file structure:
 *
 * {
 *   "defaults": {
 *     "Chat": "doubao",
 *     "FunctionCalling": "ernie"
 *   },
 *   "providers": {
 *     "doubao": {
 *       "name": "豆包",
 *       "api_key": "your_api_key",
 *       "base_url": "https://api.doubao.com",
 *       "models": {
 *         "Chat": "doubao-pro-32k"
 *       }
 *     },
 *     "ernie": {
 *       "name": "百度文心",
 *       "models": {
 *         "FunctionCalling": "ernie-bot-4"
 *       }
 *     }
 *   }
 * }
 */
class ModelProvidersConfig
{
public:
    // Constructor loads the configuration file
    ModelProvidersConfig(const QString &filePath = QString());

    // Get default provider for interface type
    QString getDefaultProvider(const QString &interfaceType) const;

    // Get all available providers
    QStringList getAvailableProviders() const;

    // Get all interface types supported by a provider
    QStringList getSupportedInterfaces(const QString &provider) const;

    // Get provider configuration for specific interface
    QVariantHash getProviderConfig(const QString &provider, const QString &interfaceType) const;

    // Get all models configured for a provider
    QStringList getProviderModels(const QString &provider) const;

    // Check if provider supports interface type
    bool supportsInterface(const QString &provider, const QString &interfaceType) const;

    // Get model name for provider and interface
    QString getModelName(const QString &provider, const QString &interfaceType) const;

    // Check if configuration is valid
    bool isValid() const;

private:
    QJsonObject modelConfig;
    bool isConfigValid = false;
};

AIDAEMON_END_NAMESPACE

#endif // MODELPROVIDERSCONFIG_H
