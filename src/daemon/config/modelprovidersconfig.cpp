// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelprovidersconfig.h"
#include "modelcenter/modelutils.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logConfig)

AIDAEMON_USE_NAMESPACE

ModelProvidersConfig::ModelProvidersConfig(const QString &filePath)
{
    QString path = filePath.isEmpty() ? ModelUtils::providerConfigPath() : filePath;
    QFile file(path);

    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qCWarning(logConfig) << "Model providers config file not found or not readable:" << path;
        isConfigValid = false;
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull()) {
        qCWarning(logConfig) << "Failed to parse model providers config file:" << error.errorString();
        isConfigValid = false;
        return;
    }

    modelConfig = doc.object();
    isConfigValid = !modelConfig.isEmpty();

    if (isConfigValid) {
        qCDebug(logConfig) << "Model providers config loaded from:" << path;
    }
}

QString ModelProvidersConfig::getDefaultProvider(const QString &interfaceType) const
{
    if (!isValid()) {
        qCWarning(logConfig) << "Configuration invalid, returning empty default provider";
        return QString();
    }

    return modelConfig["defaults"].toObject()[interfaceType].toString();
}

QStringList ModelProvidersConfig::getAvailableProviders() const
{
    if (!isValid()) {
        return QStringList();
    }

    return modelConfig["providers"].toObject().keys();
}

QStringList ModelProvidersConfig::getSupportedInterfaces(const QString &provider) const
{
    if (!isValid() || provider.isEmpty()) {
        return QStringList();
    }

    QJsonObject providerObj = modelConfig["providers"].toObject()[provider].toObject();
    if (providerObj.isEmpty() || !providerObj.contains("models")) {
        return QStringList();
    }

    return providerObj["models"].toObject().keys();
}

QVariantHash ModelProvidersConfig::getProviderConfig(const QString &provider, const QString &interfaceType) const
{
    QVariantHash config;
    if (!isValid() || provider.isEmpty()) {
        return config;
    }

    QJsonObject providerObj = modelConfig["providers"].toObject()[provider].toObject();
    if (providerObj.isEmpty()) {
        return config;
    }

    // Add provider-level keys
    for (const QString &key : providerObj.keys()) {
        if (key != "models") {
            config.insert(key, providerObj[key].toVariant());
        }
    }

    // Add model for the specific interface
    if (!interfaceType.isEmpty() && providerObj.contains("models")) {
        QJsonObject modelsObj = providerObj["models"].toObject();
        if (modelsObj.contains(interfaceType)) {
            config.insert("Model", modelsObj[interfaceType].toVariant());
        }
    }

    return config;
}

QStringList ModelProvidersConfig::getProviderModels(const QString &provider) const
{
    QStringList models;
    if (!isValid() || provider.isEmpty()) {
        return models;
    }

    QJsonObject providerObj = modelConfig["providers"].toObject()[provider].toObject();
    if (providerObj.isEmpty() || !providerObj.contains("models")) {
        return models;
    }

    QJsonObject modelsObj = providerObj["models"].toObject();
    for (const auto value : modelsObj) {
        models.append(value.toString());
    }

    return models;
}

bool ModelProvidersConfig::supportsInterface(const QString &provider, const QString &interfaceType) const
{
    if (!isValid() || provider.isEmpty() || interfaceType.isEmpty()) {
        return false;
    }

    QJsonObject providerObj = modelConfig["providers"].toObject()[provider].toObject();
    if (providerObj.isEmpty() || !providerObj.contains("models")) {
        return false;
    }

    return providerObj["models"].toObject().contains(interfaceType);
}

QString ModelProvidersConfig::getModelName(const QString &provider, const QString &interfaceType) const
{
    if (!supportsInterface(provider, interfaceType)) {
        return QString();
    }

    return modelConfig["providers"].toObject()[provider].toObject()["models"].toObject()[interfaceType].toString();
}

bool ModelProvidersConfig::isValid() const
{
    return isConfigValid;
}
