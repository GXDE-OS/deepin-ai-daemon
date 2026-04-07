// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddingmodel.h"
#include "modelcenter/providers/modelprovider.h"
#include "modelcenter/providers/doubao/doubaoprovider.h"
#include "modelcenter/providers/modelhub/modelhubprovider.h"
#include "modelcenter/providers/openai/openaiprovider.h"
#include "modelcenter/modelcenter_define.h"
#include "modelcenter/interfaces/embeddinginterface.h"

#include <QLoggingCategory>
#include <QDebug>
#include <QJsonArray>

Q_LOGGING_CATEGORY(embeddingmodel, "vectordb.embeddingmodel")

AIDAEMON_VECTORDB_USE_NAMESPACE
using namespace AIDAEMON_NAMESPACE;

EmbeddingModel::EmbeddingModel(const QString &modelId, EmbeddingInterface *interface, QObject *parent)
    : QObject(parent)
    , m_modelId(modelId)
    , m_interface(interface)
{

}

EmbeddingModel::~EmbeddingModel()
{
    if (m_interface) {
        m_interface->terminate();
        m_interface->deleteLater();
    }
}

QString EmbeddingModel::modelId() const
{
    return m_modelId;
}

bool EmbeddingModel::isValid() const
{
    return m_interface != nullptr;
}

QJsonObject EmbeddingModel::embeddings(const QStringList &texts, const QVariantHash &params)
{
    if (!isValid()) {
        qCWarning(embeddingmodel) << "EmbeddingModel: Model is not valid:" << m_modelId;
        return QJsonObject();
    }

    auto responseObj = m_interface->embeddings(texts, params);
    // Parse embeddings
    if (responseObj.contains("data")) {
        QJsonArray embeddingsArray = responseObj["data"].toArray();
        embeddingsArray = parseEmbeddingVectors(embeddingsArray);

        responseObj["embeddings"] = embeddingsArray;
    }

    return responseObj;
}

QJsonObject EmbeddingModel::embeddings(const QString &text, const QVariantHash &params)
{
    return embeddings(QStringList() << text, params);
}

void EmbeddingModel::terminate()
{
    if (m_interface) {
        m_interface->terminate();
    }
}

int EmbeddingModel::batchSize()
{
    if (!m_interface) {
        return 1;
    }

    return m_interface->batchSize();
}

QJsonArray EmbeddingModel::parseEmbeddingVectors(const QJsonArray &embeddingsArray)
{
    QJsonArray embeddings;

    for (const QJsonValue &value : embeddingsArray) {
        if (value.isObject()) {
            QJsonArray vectorArray = value["embedding"].toArray();
            embeddings.append(vectorArray);
        } else {
            embeddings.append(QJsonArray());
        }
    }

    return embeddings;
}
