// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATAPARSERTASK_H
#define DATAPARSERTASK_H

#include "../stagetask.h"

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Data parsing task
 */
class DataParserTask : public StageTask
{
    Q_OBJECT

public:
    explicit DataParserTask(const QString &taskId, const QVariantMap &params);

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
    QStringList m_files;
    int m_chunkSize = 512;
    int m_overlapSize = 50;
    QString m_outputFormat = "text_chunks";
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // DATAPARSERTASK_H
