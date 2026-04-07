// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EMBEDDINGINTERFACE_H
#define EMBEDDINGINTERFACE_H

#include "abstractinterface.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>
#include <QVector>

AIDAEMON_BEGIN_NAMESPACE

class EmbeddingInterface : public QObject, public AbstractInterface
{
    Q_OBJECT
public:
    explicit EmbeddingInterface(const QString &model, QObject *parent = nullptr);
    ~EmbeddingInterface();

    virtual QString model() const;
    virtual void setModel(const QString &);
    virtual int batchSize() const;
    InterfaceType type() override;

    /**
     * @brief Generate embeddings for multiple texts (batch processing)
     * @param texts List of input texts to embed
     * @param params Additional parameters for embedding generation
     * @return JSON response containing the embedding vectors
     */
    virtual QJsonObject embeddings(const QStringList &texts, const QVariantHash &params = {}) = 0;

    /**
     * @brief Terminate any ongoing embedding operations
     */
    virtual void terminate() = 0;
protected:
    QString modelName;
};

AIDAEMON_END_NAMESPACE

#endif // EMBEDDINGINTERFACE_H 
