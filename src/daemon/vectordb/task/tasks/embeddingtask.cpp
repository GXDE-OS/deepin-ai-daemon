// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddingtask.h"
#include "models/modelrepository.h"
#include "models/embeddingmodel.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QThread>
#include <QUuid>
#include <QJsonArray>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(embeddingTask, "aidaemon.vectordb.embeddingtask")

// Embedding Task Implementation
EmbeddingTask::EmbeddingTask(const QString &taskId, const QVariantMap &params)
    : StageTask(taskId, StageBasedTaskManager::ProcessingStage::EmbeddingGeneration)
    , m_textChunks(params.value("textChunks").toList())
    , m_modelId(params.value("modelId").toString())
{
    qCDebug(embeddingTask) << "Creating embedding task:" << taskId << "with" << m_textChunks.size() << "chunks" << "model:" << m_modelId;
}

QVariantMap EmbeddingTask::getResourceRequirements() const
{
    QVariantMap requirements;
    requirements["cpu"] = 0.6;
    requirements["memory"] = 1024; // MB
    requirements["gpu"] = 0.8; // High GPU usage if available
    requirements["network"] = 0.7; // Network for API calls
    requirements["io"] = 0.3;
    return requirements;
}

qreal EmbeddingTask::getEstimatedDuration() const
{
    // Estimate 5 seconds per chunk
    return m_textChunks.size() * 5.0;
}

QString EmbeddingTask::getResourceType() const
{
    return "gpu"; // Prefer GPU if available
}

bool EmbeddingTask::initializeExecution()
{
    qCDebug(embeddingTask) << "Initializing embedding task:" << getTaskId();
    setProgress(5);
    return true;
}

QVariantMap EmbeddingTask::executeStage()
{
    qCDebug(embeddingTask) << "Executing embedding task:" << getTaskId();
    
    QVariantMap results;
    QVariantList embeddings;
    ModelRepository respo;
    respo.refresh();
    EmbeddingModel *embedModel = respo.createModel(m_modelId, respo.getModelInfo(m_modelId));
    int batchSize = embedModel->batchSize();
    QStringList chunks = getChunks();

    QJsonArray embeddingsArray;
    for (int i = 0; i < chunks.size(); i += batchSize) {
        if (checkCancellation()) {
            break;
        }
        checkPause();

        QStringList batch = chunks.mid(i, batchSize);
        QJsonObject result = embedModel->embeddings(batch);
        for (int j = 0; j < result.value("embeddings").toArray().size(); ++j) {
            embeddingsArray.append(result.value("embeddings").toArray().at(j));
        }

        // Update progress
        int progress = 10 + (i * 80) / m_textChunks.size();
        setProgress(progress);
    }

    if (embeddingsArray.size() != m_textChunks.size()) {
        qCWarning(embeddingTask) << "embedding result error, size:" << embeddingsArray.size();
        results["success"] = false;
        results["model"] = m_modelId;
        return results;
    }

    for (int i = 0; i < m_textChunks.size(); ++i) {
        QVariantMap chunk = m_textChunks[i].toMap();
        QString chunkId = chunk.value("chunkId").toString();

        QVariantList vector;
        vector = embeddingsArray.at(i).toArray().toVariantList();
        QVariantMap embedding;
        embedding["vectorId"] = chunkId;
        embedding["vector"] = vector;

        embeddings.append(embedding);
    }
    
    results["embeddings"] = embeddings;
    results["dims"] = embeddings[0].toMap().value("vector").toList().size();
    results["embeddingCount"] = embeddings.size();  // Use embeddingCount to match validation logic
    results["model"] = m_modelId;
    results["success"] = true;
    
    setProgress(90);
    return results;
}

void EmbeddingTask::finalizeExecution()
{
    qCDebug(embeddingTask) << "Finalizing embedding task:" << getTaskId();
    setProgress(100);
}

void EmbeddingTask::cleanupResources()
{
    qCDebug(embeddingTask) << "Cleaning up embedding task resources:" << getTaskId();
}

QStringList EmbeddingTask::getChunks()
{
    QStringList results;
    for (int i = 0; i < m_textChunks.size(); ++i) {
        QVariantMap chunk = m_textChunks[i].toMap();
        results.append(chunk.value("text").toString());
    }

    return results;
}

AIDAEMON_VECTORDB_END_NAMESPACE
