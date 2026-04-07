// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SEARCHTASK_H
#define SEARCHTASK_H

#include "../stagetask.h"

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Search task
 */
class SearchTask : public StageTask
{
    Q_OBJECT

public:
    explicit SearchTask(const QString &taskId, const QVariantMap &params);

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
    QString m_query;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // SEARCHTASK_H
