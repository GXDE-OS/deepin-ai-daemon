// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "private/embeddingworker_p.h"
#include "embeddingworker.h"
#include "config/dconfigmanager.h"
#include "indexcenter/database/embeddatabase.h"
#include "index_define.h"
#include "indexcenter/index/indexmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

#include <dirent.h>
#include <sys/stat.h>

Q_DECLARE_LOGGING_CATEGORY(logIndexCenter)

EmbeddingWorkerPrivate::EmbeddingWorkerPrivate(QObject *parent)
    : QObject(parent)
{
}

void EmbeddingWorkerPrivate::init()
{
    //初始化 向量化 工作目录
    QDir dir;
    if (!dir.exists(workerDir())) {
        if (!dir.mkpath(workerDir())) {
            qCWarning(logIndexCenter) << "Failed to create directory:" << workerDir();
            return;
        }
    }

    //获取落盘索引更新时间
    QFileInfoList fileList = QDir(workerDir() + QDir::separator() + appID).entryInfoList(QDir::Files, QDir::Time);
    if (!fileList.isEmpty()) {
        auto file = fileList.last();
        indexUpdateTime = file.lastModified().toSecsSinceEpoch();
    }

    //embedding、向量索引
    embedder = new Embedding(&dataBase, &dbMtx, appID, this);
    indexer = new VectorIndex(&dataBase, &dbMtx, appID, this);

    QString databasePath;
    if (appID == kSystemAssistantKey)
        databasePath = QString("%0.db").arg(kSystemAssistantData);
    else
        databasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QDir::separator() +  appID + ".db";;
    dataBase = EmbedDBVendorIns->addDatabase(databasePath);

    embedder->createEmbedDataTable();

    if (appID == kUosAIAssistant) {
        // uos-ai 另存原文档
        m_saveAsDoc = true;
        qCDebug(logIndexCenter) << "UOS-AI mode enabled: saving original documents";
    }
}

QStringList EmbeddingWorkerPrivate::embeddingPaths()
{
    QStringList embeddingFilePaths;
    QString embeddingPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Embedding";
    QDir dir(embeddingPath);
    if (!dir.exists()) {
        qCDebug(logIndexCenter) << "Embedding directory does not exist:" << embeddingPath;
        return {};
    }

    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList embeddingList = dir.entryInfoList();

    foreach (const QFileInfo &fileInfo, embeddingList) {
        embeddingFilePaths << fileInfo.absoluteFilePath();
    }

    return embeddingFilePaths;
}


bool EmbeddingWorkerPrivate::updateIndex(const QStringList &files)
{
    if (files.isEmpty()) {
        qCDebug(logIndexCenter) << "No files to update";
        return false;
    }

    bool embedRes = true;
    for (const QString &embeddingfile : files) {
        if (m_saveAsDoc)
            embedRes &= embedder->embeddingDocumentSaveAs(embeddingfile);
        else
            embedRes &= embedder->embeddingDocument(embeddingfile);
    }

    if (!embedRes) {
        qCWarning(logIndexCenter) << "Failed to embed documents";
        embedder->embeddingClear();
        return false;
    }

    bool updateRes = indexer->updateIndex(EmbeddingDim, embedder->getEmbedVectorCache());
    if (!updateRes) {
        qCWarning(logIndexCenter) << "Failed to update vector index";
        embedder->embeddingClear();
        return false;
    }

    indexUpdateTime = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    return true;
}

bool EmbeddingWorkerPrivate::deleteIndex(const QStringList &files)
{
    qCInfo(logIndexCenter) << "Starting index deletion for" << files.size() << "files";
    QString sourceStr = "(";
    for (const QString &source : files) {
        if (files.last() == source) {
            sourceStr += "'" + source + "')";
            break;
        }
        sourceStr += "'" + source + "', ";
    }

    QList<QVariantList> statusResult;
    {
        // query doc status
        QMutexLocker lk(&dbMtx);
        QString queryDocs = "SELECT file, status FROM " + QString(kEmbeddingDBFileStatusTable) + " WHERE file IN " + sourceStr;
        EmbedDBVendorIns->executeQuery(&dataBase, queryDocs, statusResult);
    }
    for (const QVariantList &res : statusResult) {
        if (res.isEmpty())
            break;

        if (!res[0].isValid() || !res[1].isValid())
            continue;

        QString docName = res[0].toString();
        int docStatus = res[1].toInt();

        qCDebug(logIndexCenter) << "Processing document:" << docName << "with status:" << docStatus;

        switch (docStatus) {
        case EmbeddingStatus::Success:
            deleteSuccessIndex(docName);
            break;
        case EmbeddingStatus::EmbeddingStart:
            embedder->appendAbortFile(docName);
            qCDebug(logIndexCenter) << "Aborting embedding for:" << docName;
            break;
        case EmbeddingStatus::EmbeddingError:
        case EmbeddingStatus::ContentError:
            qCDebug(logIndexCenter) << "Skipping error document:" << docName;
            break;
        }

        deleteErrorIndex(docName);
    }

    return true;
}

bool EmbeddingWorkerPrivate::deleteErrorIndex(const QString &file)
{
    bool res;

    QString deleteStatus = "DELETE FROM " + QString(kEmbeddingDBFileStatusTable) + " WHERE file = " + "'" + file + "'";
    {
        QMutexLocker lk(&dbMtx);
        res = EmbedDBVendorIns->executeQuery(&dataBase, deleteStatus);
    }

    // 删除另存的文档
    if (m_saveAsDoc)
        embedder->doDeleteSaveAsDoc(QStringList(file));

    return res;
}

bool EmbeddingWorkerPrivate::deleteSuccessIndex(const QString &file)
{
    bool res = false;

    //删除缓存中的数据、重置缓存索引
    if (!embedder->getEmbedVectorCache().isEmpty()) {
        embedder->deleteCacheIndex(QStringList(file));
        indexer->resetCacheIndex(EmbeddingDim, embedder->getEmbedVectorCache());
        qCDebug(logIndexCenter) << "Cleared cache index for:" << file;
    }

    //删除已存储的数据
    QList<QVariantList> result;
    QString queryDeleteID = "SELECT id FROM " + QString(kEmbeddingDBMetaDataTable) + " WHERE source = '" + file + "'";
    QString queryDelete = "DELETE FROM " + QString(kEmbeddingDBMetaDataTable) + " WHERE source = '" + file + "'";
    {
        QMutexLocker lk(&dbMtx);
        res &= EmbedDBVendorIns->executeQuery(&dataBase, queryDeleteID, result);
        res &= EmbedDBVendorIns->executeQuery(&dataBase, queryDelete);
    }

    // 索引deleteBitSet置1
    QString idsStr = "(";
    for (const QVariantList &res : result) {
        if (res.empty())
            break;

        if (!res[0].isValid())
            continue;

        faiss::idx_t id = res[0].toInt();
        if (result.last() == res) {
            idsStr += "'" + QString::number(id) + "')";
            break;
        }
        idsStr += "'" + QString::number(id) + "', ";
    }
    QString updateBitSet = "UPDATE " + QString(kEmbeddingDBIndexSegTable) + " SET " + QString(kEmbeddingDBSegIndexTableBitSet)
                           + " = '" + QString::number(1) + "' WHERE id IN " + idsStr;
    {
        QMutexLocker lk(&dbMtx);
        res &= EmbedDBVendorIns->executeQuery(&dataBase, updateBitSet);
    }

    return res;
}

QString EmbeddingWorkerPrivate::vectorSearch(const QString &query, int topK)
{
    qCDebug(logIndexCenter) << "Performing vector search with query:" << query << "topK:" << topK;
    QVector<float> queryVector;  //查询向量 传递float指针
    embedder->embeddingQuery(query, queryVector);

    QMap<float, faiss::idx_t> cacheSearchRes;
    QMap<float, faiss::idx_t> dumpSearchRes;
    indexer->vectorSearch(topK, queryVector.data(), cacheSearchRes, dumpSearchRes);
    QString res = embedder->loadTextsFromSearch(topK, cacheSearchRes, dumpSearchRes);
    qCDebug(logIndexCenter) << "Vector search completed with" << cacheSearchRes.size() + dumpSearchRes.size() << "results";
    return res;
}

QString EmbeddingWorkerPrivate::indexDir()
{
    return workerDir() + QDir::separator() + appID;
}

QString EmbeddingWorkerPrivate::getIndexDocs()
{
    QJsonObject resultObj;
    resultObj["version"] = GET_DOCS_VERSION;
    QJsonArray resultArray;

    QList<QVariantList> statusResult;
    {
        // query doc status
        QMutexLocker lk(&dbMtx);
        QString queryDocs = "SELECT file, status FROM " + QString(kEmbeddingDBFileStatusTable);
        EmbedDBVendorIns->executeQuery(&dataBase, queryDocs, statusResult);
    }

    qCDebug(logIndexCenter) << "Found" << statusResult.size() << "documents in status table";
    QStringList statusTableFiles;
    for (const QVariantList &res : statusResult) {
        if (res.isEmpty())
            break;

        if (!res[0].isValid() || !res[1].isValid())
            continue;

        QString docName = res[0].toString();
        int docStatus = res[1].toInt();

        statusTableFiles.append(docName);

        QJsonObject obj;
        obj.insert("doc", docName);
        obj.insert("status", docStatus);

        if (docStatus == EmbeddingStatus::Success) {
            QMap<faiss::idx_t, QPair<QString, QString>> cacheData = embedder->getEmbedDataCache();
            for (faiss::idx_t id : cacheData.keys()) {
                if (cacheData.value(id).first == docName) {
                    obj.insert("content", cacheData.value(id).second);
                    break;
                }
            }

            QList<QVariantList> result;
            {
                QMutexLocker lk(&dbMtx);
                QString queryDocs = "SELECT content FROM " + QString(kEmbeddingDBMetaDataTable) + " WHERE source = " + "'" + docName + "' LIMIT 1";
                EmbedDBVendorIns->executeQuery(&dataBase, queryDocs, result);
            }
            for (const QVariantList &res : result) {
                if (res.isEmpty())
                    break;

                if (!res[0].isValid())
                    continue;

                obj.insert("content", res[0].toString());
            }
        }

        resultArray.append(obj);
    }

    resultObj.insert("result", resultArray);
    return QJsonDocument(resultObj).toJson(QJsonDocument::Compact);
}

bool EmbeddingWorkerPrivate::isSupportDoc(const QString &file)
{
    QFileInfo fileInfo(file);
    QStringList suffixs;
    suffixs << "txt"
            << "text"
            << "doc"
            << "docx"
            << "xls"
            << "xlsx"
            << "ppt"
            << "pptx"
            << "pps"
            << "ppsx"
            << "pdf"
            << "wps"
            << "dps";

    return suffixs.contains(fileInfo.suffix());
}

bool EmbeddingWorkerPrivate::isFilter(const QString &file)
{
    const auto &blacklist = ConfigManagerIns->value(BLACKLIST_GROUP, BLACKLIST_PATHS, QStringList()).toStringList();
    return std::any_of(blacklist.begin(), blacklist.end(), [&file](const QString &path) {
            return file.startsWith(path);
    });
}

EmbeddingWorker::EmbeddingWorker(const QString &appid, QObject *parent)
    : QObject(parent),
      d(new EmbeddingWorkerPrivate(this))
{
    Q_ASSERT(!appid.isEmpty());

    d->appID = appid;
    d->init();

    moveToThread(&d->workThread);
    d->workThread.start();

    connect(this, &EmbeddingWorker::stopEmbedding, this, &EmbeddingWorker::doIndexDump);
    connect(d->indexer, &VectorIndex::indexDump, this, &EmbeddingWorker::doIndexDump);

    dumpTimer.setInterval(30000); // 30秒落一次盘
    dumpTimer.setSingleShot(false);
    connect(&dumpTimer, &QTimer::timeout, this, &EmbeddingWorker::doIndexDump);
    dumpTimer.start(10000);

    //索引建立成功，完成后续操作
    //connect(this, &EmbeddingWorker::indexCreateSuccess, d->embedder, &Embedding::onIndexCreateSuccess);
}

EmbeddingWorker::~EmbeddingWorker()
{
    // 已建索引落盘、数据存储
    doIndexDump();

    if (d->embedder) {
        delete d->embedder;
        d->embedder = nullptr;
    }

    if (d->indexer) {
        delete d->indexer;
        d->indexer = nullptr;
    }

    EmbedDBVendor::instance()->removeDatabase(&d->dataBase);

    //TODO:停止embeddding、已建索引落盘、数据存储等
}

void EmbeddingWorker::setEmbeddingApi(embeddingApi api, void *user)
{
    d->embedder->setEmbeddingApi(api, user);
}

void EmbeddingWorker::stop()
{
    d->m_creatingAll = false;
    Q_EMIT stopEmbedding();
}

void EmbeddingWorker::saveAllIndex()
{
    Q_EMIT stopEmbedding();
}

int EmbeddingWorker::createAllState()
{
    return  d->m_creatingAll ? GET_INDEX_STATUS_CODE(INDEX_STATUS_CREATING) : GET_INDEX_STATUS_CODE(INDEX_STATUS_SUCCESS);
}

void EmbeddingWorker::setWatch(bool watch)
{
    auto idx = IndexManager::instance();
    Q_ASSERT(idx);
    disconnect(idx, nullptr, this, nullptr);
    if (watch) {
        connect(idx, &IndexManager::fileCreated, this, &EmbeddingWorker::onFileMonitorCreate);
        connect(idx, &IndexManager::fileDeleted, this, &EmbeddingWorker::onFileMonitorDelete);
        //connect(idx, &IndexManager::fileAttributeChanged, this, );
    }
}

qint64 EmbeddingWorker::getIndexUpdateTime()
{
    return d->indexUpdateTime;
}

bool EmbeddingWorker::doCreateIndex(const QStringList &files)
{
    int ret = d->updateIndex(files);
    Q_EMIT statusChanged(d->appID, QStringList(), ret);

    return ret;
}

bool EmbeddingWorker::doDeleteIndex(const QStringList &files)
{
    bool ret = d->deleteIndex(files);
    if (!ret)
        qCWarning(logIndexCenter) << "Failed to delete index";
    else
        Q_EMIT indexDeleted(d->appID, files);

    return ret;
}

void EmbeddingWorker::onFileMonitorCreate(const QString &file)
{
    doCreateIndex(QStringList(file));
}

void EmbeddingWorker::onFileMonitorDelete(const QString &file)
{
    doDeleteIndex(QStringList(file));
}

void EmbeddingWorker::doIndexDump()
{
    faiss::idx_t startID = d->indexer->getDumpIndexIDRange().first;
    faiss::idx_t endID = d->indexer->getDumpIndexIDRange().second;

    if (startID > endID)
        return;

    if (d->embedder->doIndexDump(startID, endID)) {
        d->indexer->doIndexDump();
    }
}

void EmbeddingWorker::onCreateAllIndex()
{
    d->m_creatingAll = true;
    QString path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    traverseAndCreate(path);
    d->m_creatingAll = false;
}

void EmbeddingWorker::traverseAndCreate(const QString &path)
{
    if (!d->m_creatingAll || d->isFilter(path))
        return;

    const std::string tmp = path.toStdString();
    const char *filePath = tmp.c_str();
    struct stat st;
    if (stat(filePath, &st) != 0)
        return;

    if (S_ISDIR(st.st_mode)) {
        DIR *dir = nullptr;
        if (!(dir = opendir(filePath))) {
            qCWarning(logIndexCenter) << "Failed to open directory:" << filePath;
            return;
        }

        struct dirent *dent = nullptr;
        char fn[FILENAME_MAX] = { 0 };
        strcpy(fn, filePath);
        size_t len = strlen(filePath);
        if (strcmp(filePath, "/"))
            fn[len++] = '/';

        // traverse
        while ((dent = readdir(dir)) && d->m_creatingAll) {
            if (dent->d_name[0] == '.')
                continue;

            if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
                continue;

            strncpy(fn + len, dent->d_name, FILENAME_MAX - len);
            traverseAndCreate(fn);
        }

        if (dir)
            closedir(dir);
        return;
    }

    static const int maxFileSize = 50 * 1024 * 1024; //50MB
    if (QFileInfo(path).size() > maxFileSize)
        return;

    if (d->m_creatingAll)
        doCreateIndex({path});
}

QString EmbeddingWorker::doVectorSearch(const QString &query, int topK)
{
    if (query.isEmpty()) {
        qCWarning(logIndexCenter) << "Empty query received";
        return {};
    }
    return d->vectorSearch(query,topK);
}

QString EmbeddingWorker::getDocFile()
{
    return d->getIndexDocs();
}
