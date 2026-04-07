// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PERSISTENCETASK_H
#define PERSISTENCETASK_H

#include "../stagetask.h"

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Data persistence task
 */
class PersistenceTask : public StageTask
{
    Q_OBJECT

public:
    explicit PersistenceTask(const QString &taskId, const QVariantMap &data);

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
    QVariantMap m_data;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // PERSISTENCETASK_H
