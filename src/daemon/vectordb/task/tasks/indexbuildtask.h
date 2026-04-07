// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INDEXBUILDTASK_H
#define INDEXBUILDTASK_H

#include "../stagetask.h"

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class VectorIndexManager;

/**
 * @brief Index building task
 */
class IndexBuildTask : public StageTask
{
    Q_OBJECT

public:
    explicit IndexBuildTask(const QString &taskId, const QVariantMap &params);

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
    QVariantList m_vectors;
    QString m_indexId;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // INDEXBUILDTASK_H
