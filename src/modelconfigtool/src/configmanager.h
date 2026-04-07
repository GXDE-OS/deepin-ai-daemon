// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QJsonObject>

struct ModelConfig {
    QString provider;
    QString modelType;
    QMap<QString, QString> parameters;
    bool enabled = true;
};

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);

    // Load and save configuration
    bool loadConfig(const QString &filePath);
    bool saveConfig(const QString &filePath);

    // Model management
    QStringList getModelTypes() const;
    QStringList getProvidersForType(const QString &modelType) const;
    QList<ModelConfig> getModelsForType(const QString &modelType) const;

    void addModel(const ModelConfig &config);
    void updateModel(int index, const ModelConfig &config);
    void removeModel(const QString &modelType, int index);

    // Provider information
    QStringList getProviderParameters(const QString &provider, const QString &modelType) const;
    QString getProviderName(const QString &provider) const;

    // Default providers
    QString getDefaultProvider(const QString &modelType) const;
    void setDefaultProvider(const QString &modelType, const QString &provider);
    void updateModelEnabledStatus(const QString &modelType);

private:
    QJsonObject rootObject;
    QMap<QString, QList<ModelConfig>> models;
    QMap<QString, QString> defaultProviders;
    QMap<QString, QString> providerNames;

    void parseConfig();
    QStringList getKnownParameters(const QString &provider, const QString &modelType) const;
    QStringList getDefaultParameters(const QString &provider, const QString &modelType) const;
    QStringList getDefaultProvidersForType(const QString &modelType) const;
};

#endif // CONFIGMANAGER_H
