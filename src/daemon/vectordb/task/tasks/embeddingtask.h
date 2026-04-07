// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EMBEDDINGTASK_H
#define EMBEDDINGTASK_H

#include "../stagetask.h"

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Embedding generation task
 */
class EmbeddingTask : public StageTask
{
    Q_OBJECT

public:
    explicit EmbeddingTask(const QString &taskId, const QVariantMap &params);

    // StageTask interface
    QVariantMap getResourceRequirements() const override;
    qreal getEstimatedDuration() const override;
    QString getResourceType() const override;

protected:
    bool initializeExecution() override;
    QVariantMap executeStage() override;
    void finalizeExecution() override;
    void cleanupResources() override;

private:
    QVariantList m_textChunks;
    QString m_modelId;

    QStringList getChunks();
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // EMBEDDINGTASK_H
