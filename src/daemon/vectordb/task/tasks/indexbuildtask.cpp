// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexbuildtask.h"
#include "../../index/vectorindexmanager.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>
#include <QtCore/QUuid>
#include <QStandardPaths>
#include <QDir>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(indexBuildTask, "aidaemon.vectordb.indexbuildtask")

// Index Build Task Implementation
IndexBuildTask::IndexBuildTask(const QString &taskId, const QVariantMap &params)
    : StageTask(taskId, StageBasedTaskManager::ProcessingStage::IndexBuilding)
    , m_vectors(params.value("vectors").toList())
{
    qCDebug(indexBuildTask) << "Creating index build task:" << taskId << "with" << params.size() << "vectors, indexId:" << m_indexId;
}

QVariantMap IndexBuildTask::getResourceRequirements() const
{
    QVariantMap requirements;
    requirements["cpu"] = 0.7;
    requirements["memory"] = 2048; // MB - High memory for index building
    requirements["gpu"] = 0.0;
    requirements["network"] = 0.1;
    requirements["io"] = 0.8; // High I/O for index writing
    return requirements;
}

qreal IndexBuildTask::getEstimatedDuration() const
{
    // Estimate 0.1 seconds per vector
    return m_vectors.size() * 0.1;
}

QString IndexBuildTask::getResourceType() const
{
    return "memory";
}

bool IndexBuildTask::initializeExecution()
{
    qCDebug(indexBuildTask) << "Initializing index build task:" << getTaskId();
    setProgress(5);
    return true;
}

QVariantMap IndexBuildTask::executeStage()
{
    qCDebug(indexBuildTask) << "Executing index build task:" << getTaskId();
    
    QVariantMap results;
    
//    try {
//        // Create VectorIndexManager for this project
//        QString absProjectDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + m_projectInfo["dataDirectory"].toString();
//        VectorIndexManager indexManager(absProjectDir);
        
//        // Phase 1: Initialize index structure
//        setProgress(20);
//        qCDebug(indexBuildTask) << "Creating index:" << m_indexId;
        
//        // Determine vector dimension from first vector
//        int dimension = 768; // Default dimension
//        if (!m_vectors.isEmpty()) {
//            QVariantMap firstVector = m_vectors.first().toMap();
//            if (firstVector.contains("vector")) {
//                QVariantList vectorData = firstVector["vector"].toList();
//                dimension = vectorData.size();
//            }
//        }
        
//        // Configure index parameters
//        QVariantMap indexConfig;
//        indexConfig["type"] = static_cast<int>(VectorIndexManager::IndexType::HNSW);
//        indexConfig["dimension"] = dimension;
//        indexConfig["metric"] = static_cast<int>(VectorIndexManager::DistanceMetric::Cosine);
//        indexConfig["M"] = 16; // HNSW parameter
//        indexConfig["efConstruction"] = 200; // HNSW parameter
        
//        bool indexCreated = indexManager.createIndex(m_indexId, indexConfig);
//        if (!indexCreated) {
//            qCWarning(indexBuildTask) << "Failed to create index:" << m_indexId;
//            results["error"] = "Failed to create index";
//            return results;
//        }
        
//        if (checkCancellation()) return results;
//        checkPause();
        
//        // Phase 2: Add vectors to index
//        setProgress(40);
//        qCDebug(indexBuildTask) << "Adding" << m_vectors.size() << "vectors to index:" << m_indexId;
        
//        bool vectorsAdded = indexManager.addVectors(m_indexId, m_vectors);
//        if (!vectorsAdded) {
//            qCWarning(indexBuildTask) << "Failed to add vectors to index:" << m_indexId;
//            results["error"] = "Failed to add vectors to index";
//            return results;
//        }
        
//        if (checkCancellation()) return results;
//        checkPause();
        
//        if (checkCancellation()) return results;
//        checkPause();
        
//        // Phase 3: Validate and get index information
//        setProgress(90);
//        qCDebug(indexBuildTask) << "Validating index:" << m_indexId;

//        indexManager.saveIndexToFile(m_indexId);

//        // Get index information
//        QVariantMap indexInfo = indexManager.getIndexInfo(m_indexId);
//        QVariantMap indexStats = indexManager.getIndexStatistics(m_indexId);
//        qint64 indexSize = indexManager.getIndexSize(m_indexId);
        
//        // Prepare results
//        results["indexId"] = m_indexId;
//        results["projectId"] = m_projectInfo.value("projectId").toString();;
//        results["indexedVectors"] = m_vectors.size();
//        results["indexType"] = "HNSW";
//        results["dimension"] = dimension;
//        results["indexSize"] = indexSize;
//        results["indexInfo"] = indexInfo;
//        results["indexStatistics"] = indexStats;
        
//        qCDebug(indexBuildTask) << "Index build completed successfully:" << m_indexId
//                               << "vectors:" << m_vectors.size() << "size:" << indexSize;
        
//    } catch (const std::exception &e) {
//        qCCritical(indexBuildTask) << "Exception during index building:" << e.what();
//        results["error"] = QString("Exception: %1").arg(e.what());
//        return results;
//    } catch (...) {
//        qCCritical(indexBuildTask) << "Unknown exception during index building";
//        results["error"] = "Unknown exception occurred";
//        return results;
//    }
    
    return results;
}

void IndexBuildTask::finalizeExecution()
{
    qCDebug(indexBuildTask) << "Finalizing index build task:" << getTaskId();
    setProgress(100);
}

void IndexBuildTask::cleanupResources()
{
    qCDebug(indexBuildTask) << "Cleaning up index build task resources:" << getTaskId();
    
    // Note: We don't delete the index here as it should persist after task completion
    // The index will be managed by the VectorIndexManager and project lifecycle
//    qCDebug(indexBuildTask) << "Index" << m_indexId << "will persist for project:" << m_projectId;
}

AIDAEMON_VECTORDB_END_NAMESPACE
