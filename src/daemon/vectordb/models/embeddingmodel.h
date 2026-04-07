// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EMBEDDINGMODEL_H
#define EMBEDDINGMODEL_H

#include "aidaemon_global.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>
#include <QStringList>

AIDAEMON_BEGIN_NAMESPACE
class EmbeddingInterface;
AIDAEMON_END_NAMESPACE

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Embedding model wrapper class for text embedding operations
 * 
 * This class provides a high-level interface for text embedding operations
 * by wrapping the underlying EmbeddingInterface implementation.
 * It handles model initialization and provides convenient methods for
 * generating embeddings from text inputs.
 */
class EmbeddingModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param modelId Model identifier (format: "provider/modelname")
     * @param modelInfo Model configuration information
     * @param parent Parent QObject
     */
    explicit EmbeddingModel(const QString &modelId, EmbeddingInterface *interface, QObject *parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~EmbeddingModel();

    /**
     * @brief Get the model ID
     * @return Model ID string
     */
    QString modelId() const;

    /**
     * @brief Check if the model is valid and ready to use
     * @return true if model is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Generate embeddings for multiple texts (batch processing)
     * @param texts List of input texts to embed
     * @param params Additional parameters for embedding generation
     * @return JSON response containing the embedding vectors
     */
    QJsonObject embeddings(const QStringList &texts, const QVariantHash &params = {});

    /**
     * @brief Generate embeddings for a single text
     * @param text Input text to embed
     * @param params Additional parameters for embedding generation
     * @return JSON response containing the embedding vector
     */
    QJsonObject embeddings(const QString &text, const QVariantHash &params = {});

    /**
     * @brief Terminate any ongoing embedding operations
     */
    void terminate();

    /**
     * @brief embedding interface batch size
     */
    int batchSize();
protected:
    QJsonArray parseEmbeddingVectors(const QJsonArray &embeddingsArray);
protected:
    QString m_modelId;
    EmbeddingInterface *m_interface;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // EMBEDDINGMODEL_H
