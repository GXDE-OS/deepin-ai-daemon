// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexmanager.h"
#include "config/dconfigmanager.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logIndexCenter)

static IndexManager *kIndexManager = nullptr;

IndexManager::IndexManager(QObject *parent)
    : QObject(parent),
      workThread(new QThread),
      worker(new IndexWorker)
{
    Q_ASSERT(kIndexManager == nullptr);
    kIndexManager = this;

    init();
}

IndexManager::~IndexManager()
{
    if (kIndexManager == this)
        kIndexManager = nullptr;

    if (workThread->isRunning()) {
        worker->stop();

        workThread->quit();
        workThread->wait();
    }

    qCInfo(logIndexCenter) << "Index manager has been terminated";
}

IndexManager *IndexManager::instance()
{
    return kIndexManager;
}

void IndexManager::init()
{
    worker->moveToThread(workThread.data());
    workThread->start();
}

void IndexManager::onSemanticAnalysisChecked(bool isChecked, bool isFromUser)
{
    qCDebug(logIndexCenter) << "Semantic analysis status change requested - Current:" << isSemanticOn 
                           << "New:" << isChecked << "FromUser:" << isFromUser;

    QMutexLocker lock(&semanticOnMutex);
    if (isSemanticOn == isChecked) {
        return;
    }

    if (isChecked) {
        isSemanticOn = true;
        ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, ENABLE_SEMANTIC_ANALYSIS, isSemanticOn);
        connect(this, &IndexManager::createAllIndex, worker.data(), &IndexWorker::onCreateAllIndex, Qt::UniqueConnection);
        connect(this, &IndexManager::fileCreated, worker.data(), &IndexWorker::onFileCreated, Qt::UniqueConnection);
        connect(this, &IndexManager::fileAttributeChanged, worker.data(), &IndexWorker::onFileAttributeChanged, Qt::UniqueConnection);
        connect(this, &IndexManager::fileDeleted, worker.data(), &IndexWorker::onFileDeleted, Qt::UniqueConnection);
        worker->start();
        if (isFromUser || !ConfigManagerIns->value(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED, false).toBool()) {
            lock.unlock();
            emit createAllIndex();
        }
        return;
    }

    worker->stop();
    disconnect(this, &IndexManager::createAllIndex, worker.data(), &IndexWorker::onCreateAllIndex);
    disconnect(this, &IndexManager::fileCreated, worker.data(), &IndexWorker::onFileCreated);
    disconnect(this, &IndexManager::fileAttributeChanged, worker.data(), &IndexWorker::onFileAttributeChanged);
    disconnect(this, &IndexManager::fileDeleted, worker.data(), &IndexWorker::onFileDeleted);
    isSemanticOn = false;
    ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, ENABLE_SEMANTIC_ANALYSIS, isSemanticOn);
}
