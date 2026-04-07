// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELREPOSITORY_H
#define MODELREPOSITORY_H

#include "aidaemon_global.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantHash>
#include <QReadWriteLock>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class EmbeddingModel;

/**
 * @brief Model repository class for managing embedding models
 * 
 * This class is responsible for:
 * - Loading model account information during initialization
 * - Querying installed models from ModelhubWrapper
 * - Filtering models with "Embedding" architecture
 * - Providing available embedding models with proper model IDs
 * - Creating EmbeddingModel instances for text embedding operations
 */
class ModelRepository : public QObject
{
    Q_OBJECT

public:
    explicit ModelRepository(QObject *parent = nullptr);
    ~ModelRepository();

    /**
     * @brief Get the default embedding model ID
     * @return Default model ID string, empty if no models available
     */
    QString defaultModel();

    /**
     * @brief Get detailed information about a specific model
     * @param modelId Model ID in format "modelhub/ModelName"
     * @return Model information hash, empty if not found
     */
    QVariantHash getModelInfo(const QString &modelId) const;

    /**
     * @brief Get list of all available model IDs
     * @return List of model IDs as strings
     */
    QStringList getModelList() const;

    /**
     * @brief Refresh the model repository data
     * @return true if refresh successful, false otherwise
     */
    bool refresh();

    /**
     * @brief Check if a model exists in the repository
     * @param modelId Model ID to check (format: "provider/modelname")
     * @return true if model exists, false otherwise
     */
    bool hasModel(const QString &modelId) const;

    /**
     * @brief Create an EmbeddingModel instance for the specified model
     * @param modelId Model ID in format "provider/modelname"
     * @param modelInfo Model configuration information (optional, will be retrieved if not provided)
     * @return Pointer to EmbeddingModel instance, nullptr if creation failed
     */
    EmbeddingModel *createModel(const QString &modelId, const QVariantHash &modelInfo);

    /**
     * @brief Convert model ID to model name.
     * @param model ID
     * @return provider and model name
     */
    QPair<QString, QString> fromModelId(const QString &modelId) const;
private:
    /**
     * @brief Load model account information
     * @return true if loading successful, false otherwise
     */
    bool loadModelAccountInfo();

    /**
     * @brief Query installed models from ModelhubWrapper
     * @return true if query successful, false otherwise
     */
    bool queryInstalledModels();

private:
    mutable QReadWriteLock m_lock;
    QHash<QString, QVariantHash> m_modelInfo;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // MODELREPOSITORY_H 
