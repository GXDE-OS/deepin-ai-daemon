// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "searchtask.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(searchTask, "aidaemon.vectordb.searchtask")

// Search Task Implementation
SearchTask::SearchTask(const QString &taskId, const QVariantMap &params)
    : StageTask(taskId, StageBasedTaskManager::ProcessingStage::Search)
    , m_query(params.value("query").toString())
{
    qCDebug(searchTask) << "Creating search task:" << taskId << "for query:" << m_query;
}

QVariantMap SearchTask::getResourceRequirements() const
{
    QVariantMap requirements;
    requirements["cpu"] = 0.9; // High CPU for search
    requirements["memory"] = 512; // MB
    requirements["gpu"] = 0.0;
    requirements["network"] = 0.2;
    requirements["io"] = 0.4;
    return requirements;
}

qreal SearchTask::getEstimatedDuration() const
{
    return 2.0; // 2 seconds for search
}

QString SearchTask::getResourceType() const
{
    return "cpu";
}

bool SearchTask::initializeExecution()
{
    qCDebug(searchTask) << "Initializing search task:" << getTaskId();
    setProgress(10);
    return true;
}

QVariantMap SearchTask::executeStage()
{
    qCDebug(searchTask) << "Executing search task:" << getTaskId() << "query:" << m_query;
    
    QVariantMap results;
    
    setProgress(30);
    QThread::msleep(100); // Generate query embedding
    
    if (checkCancellation()) return results;
    checkPause();
    
    setProgress(60);
    QThread::msleep(150); // Perform vector search
    
    if (checkCancellation()) return results;
    checkPause();
    
    setProgress(90);
    QThread::msleep(50); // Rank and format results
    
    // Create mock search results
    QVariantList searchResults;
    for (int i = 0; i < 10; ++i) {
        QVariantMap result;
        result["id"] = QString("doc_%1").arg(i);
        result["score"] = 0.9 - (i * 0.05);
        result["title"] = QString("Document %1").arg(i);
        result["snippet"] = QString("This is a snippet from document %1 matching query: %2").arg(i).arg(m_query);
        searchResults.append(result);
    }
    
    results["query"] = m_query;
    results["searchResults"] = searchResults;
    results["resultCount"] = searchResults.size();
    results["searchTime"] = 0.15; // seconds
    
    return results;
}

void SearchTask::finalizeExecution()
{
    qCDebug(searchTask) << "Finalizing search task:" << getTaskId();
    setProgress(100);
}

void SearchTask::cleanupResources()
{
    qCDebug(searchTask) << "Cleaning up search task resources:" << getTaskId();
}

AIDAEMON_VECTORDB_END_NAMESPACE
