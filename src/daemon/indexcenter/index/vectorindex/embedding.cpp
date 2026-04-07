// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "embedding.h"
#include "vectorindex.h"
#include "indexcenter/database/embeddatabase.h"
#include "indexcenter/index/index_define.h"
#include "utils/utils.h"
#include "qt5_6_compatible.h"
#include "parsers/parserfactory.h"

using namespace ai_daemon::vector_db;

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QtConcurrent/QtConcurrent>

#include <docparser.h>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logIndexCenter)

AIDAEMON_USE_NAMESPACE

static constexpr char kSearchResultDistance[] { "distance" };

Embedding::Embedding(QSqlDatabase *db, QMutex *mtx, const QString &appID, QObject *parent)
    : QObject(parent)
    , dataBase(db)
    , dbMtx(mtx)
    , appID(appID)
{
    Q_ASSERT(db);
    Q_ASSERT(mtx);
}

bool Embedding::embeddingDocument(const QString &docFilePath)
{
    QFileInfo docFile(docFilePath);
    if (!docFile.exists()) {
        qCWarning(logIndexCenter) << "Document file not exist:" << docFilePath;
        return false;
    }

    if (isDupDocument(docFilePath)) {
        qCWarning(logIndexCenter) << "Document is duplicate:" << docFilePath;
        return false;
    }

    for (auto id : embedDataCache.keys()) {
        if (docFilePath == embedDataCache.value(id).first) {
            qCWarning(logIndexCenter) << "Document already in cache:" << docFilePath;
            return false;
        }
    }

    QString contents = parserDoc(docFilePath);
    if (contents.isEmpty())
        return false;

    //文本分块
    QStringList chunks = textsSpliter(contents);

    // 文件名大于14字节建索引
    if (docFile.baseName().toUtf8().size() > 14) {
        chunks.prepend(docFile.fileName());
    }

    if (chunks.isEmpty())
        return false;

    qCInfo(logIndexCenter) << "Start embedding document:" << docFilePath << "with" << chunks.size() << "chunks";

    // 只需前100个
    if (chunks.size() > 100) {
        chunks = chunks.mid(0, 100);
        qCDebug(logIndexCenter) << "Truncated to top 100 chunks for document:" << docFilePath;
    }

    //向量化文本块，生成向量vector
    QVector<QVector<float>> vectors;
    vectors = embeddingTexts(chunks, docFilePath);

    if (vectors.count() != chunks.count()) {
        qCWarning(logIndexCenter) << "Vector count mismatch for document:" << docFilePath;
        return false;
    }
    if (vectors.isEmpty()) {
        qCWarning(logIndexCenter) << "No vectors generated for document:" << docFilePath;
        return false;
    }

    {
        QMutexLocker lk(&embeddingMutex);
        //元数据、文本存储
        int continueID = embedDataCache.size() + getDBLastID();
        qCInfo(logIndexCenter) << "Starting embedding with ID:" << continueID;

        for (int i = 0; i < chunks.count(); i++) {
            if (chunks[i].isEmpty())
                continue;

            embedDataCache.insert(continueID, QPair<QString, QString>(docFilePath, chunks[i]));
            embedVectorCache.insert(continueID, vectors[i]);

            continueID += 1;
        }
    }
    return true;
}

bool Embedding::embeddingDocumentSaveAs(const QString &docFilePath)
{
    // Embedding SaveAs
    // uos-ai
    QString newDocPath = saveAsDocPath(docFilePath);

    if (isDupDocument(newDocPath)) {
        qCWarning(logIndexCenter) << "Document is duplicate:" << newDocPath;
        return false;
    }

    for (auto id : embedDataCache.keys()) {
        if (newDocPath == embedDataCache.value(id).first) {
            qCWarning(logIndexCenter) << "Document already in cache:" << newDocPath;
            return false;
        }
    }

    setEmbeddingStatus(newDocPath, EmbeddingStatus::EmbeddingStart);
    doSaveAsDoc(docFilePath);

    QFileInfo docFile(docFilePath);
    if (!docFile.exists()) {
        qCWarning(logIndexCenter) << "Document file not exist:" << docFilePath;
        setEmbeddingStatus(newDocPath, EmbeddingStatus::ContentError);
        return false;
    }

    QString contents = parserDoc(docFilePath);
    if (contents.isEmpty()) {
        setEmbeddingStatus(newDocPath, EmbeddingStatus::ContentError);
        return false;
    }

    //文本分块
    QStringList chunks = textsSpliter(contents);

    qCInfo(logIndexCenter) << "Start embedding document:" << newDocPath;
    //向量化文本块，生成向量vector
    QVector<QVector<float>> vectors = embeddingTexts(chunks, newDocPath);
    if (abortFiles.contains(newDocPath)) {
        qCInfo(logIndexCenter) << "Embedding aborted for document:" << newDocPath;
        abortFiles.removeAll(newDocPath);
        return false;
    }

    if (vectors.count() != chunks.count()) {
        qCWarning(logIndexCenter) << "Vector count mismatch for document:" << newDocPath;
        setEmbeddingStatus(newDocPath, EmbeddingStatus::EmbeddingError);
        return false;
    }

    if (vectors.isEmpty()) {
        qCWarning(logIndexCenter) << "No vectors generated for document:" << newDocPath;
        setEmbeddingStatus(newDocPath, EmbeddingStatus::EmbeddingError);
        return false;
    }

    {
        QMutexLocker lk(&embeddingMutex);
        //元数据、文本存储
        int continueID = embedDataCache.size() + getDBLastID();
        qCInfo(logIndexCenter) << "Starting embedding with ID:" << continueID;

        for (int i = 0; i < chunks.count(); i++) {
            if (chunks[i].isEmpty())
                continue;

            embedDataCache.insert(continueID, QPair<QString, QString>(newDocPath, chunks[i]));
            embedVectorCache.insert(continueID, vectors[i]);

            continueID += 1;
        }
    }

    setEmbeddingStatus(newDocPath, EmbeddingStatus::Success);
    return true;
}

QVector<QVector<float>> Embedding::embeddingTexts(const QStringList &texts, const QString &file)
{
    if (texts.isEmpty()) {
        qCDebug(logIndexCenter) << "Empty text list for embedding";
        return {};
    }

    int inputBatch = 15;
    QVector<QVector<float>> vectors;
    QStringList splitProcessText = texts;
    int currentIndex = 0;
    while (currentIndex < splitProcessText.size()) {
        if (abortFiles.contains(file)) {
            qCInfo(logIndexCenter) << "Embedding process aborted for file:" << file;
            break;
        }
        QStringList subList = splitProcessText.mid(currentIndex, inputBatch);
        currentIndex += inputBatch;

        QJsonObject emdObject = onHttpEmbedding(subList, apiData);
        QJsonArray embeddingsArray = emdObject["data"].toArray();
        for(auto embeddingObject : embeddingsArray) {
            QJsonArray vectorArray = embeddingObject.toObject()["embedding"].toArray();
            QVector<float> vectorTmp;
            for (auto value : vectorArray) {
                vectorTmp << static_cast<float>(value.toDouble());
            }
            vectors << vectorTmp;
        }
    }
    return vectors;
}

void Embedding::embeddingQuery(const QString &query, QVector<float> &queryVector)
{
    /* query:查询问题
     * queryVector:问题向量化
     *
     * 调用接口将query进行向量化，结果通过queryVector传递float指针
    */

    QStringList queryTexts;
    queryTexts << "为这个句子生成表示以用于检索相关文章:" + query;
    QJsonObject emdObject;
    emdObject = onHttpEmbedding(queryTexts, apiData);

    //获取query
    //local
    QJsonArray embeddingsArray = emdObject["data"].toArray();
    for(auto embeddingObject : embeddingsArray) {
        QJsonArray vectorArray = embeddingObject.toObject()["embedding"].toArray();
        for (auto value : vectorArray) {
            queryVector << static_cast<float>(value.toDouble());
        }
    }
}

bool Embedding::batchInsertDataToDB(const QStringList &inserQuery)
{
    if (inserQuery.isEmpty())
        return false;

    QMutexLocker lk(dbMtx);
    bool ok = EmbedDBVendorIns->commitTransaction(dataBase, inserQuery);
    return ok;
}

int Embedding::getDBLastID()
{
    QList<QVariantList> result;

    {
        QString query = "SELECT id FROM " + QString(kEmbeddingDBIndexSegTable) + " ORDER BY id DESC LIMIT 1";
        QMutexLocker lk(dbMtx);
        EmbedDBVendorIns->executeQuery(dataBase, query, result);
    }

    if (result.isEmpty())
        return 0;

    if (!result[0][0].isValid())
        return 0;

    return result[0][0].toInt() + 1;
}

void Embedding::createEmbedDataTable()
{
    qCInfo(logIndexCenter) << "Creating database tables";

    QString createTable1SQL = "CREATE TABLE IF NOT EXISTS " + QString(kEmbeddingDBMetaDataTable) + " (id INTEGER PRIMARY KEY, source TEXT, content TEXT)";
    QString createTable2SQL = "CREATE TABLE IF NOT EXISTS " + QString(kEmbeddingDBIndexSegTable) + " (id INTEGER PRIMARY KEY, deleteBit INTEGER, content TEXT)";
    QString createTable3SQL = "CREATE TABLE IF NOT EXISTS " + QString(kEmbeddingDBFileStatusTable) + " (file TEXT PRIMARY KEY, status INTEGER)";

    {
        QMutexLocker lk(dbMtx);
        EmbedDBVendorIns->executeQuery(dataBase, createTable1SQL);
        EmbedDBVendorIns->executeQuery(dataBase, createTable2SQL);
        EmbedDBVendorIns->executeQuery(dataBase, createTable3SQL);
    }

    if (appID == kUosAIAssistant)
        initDataStatus();

    return ;
}

bool Embedding::isDupDocument(const QString &docFilePath)
{
    QList<QVariantList> result;

    QString query = "SELECT CASE WHEN EXISTS (SELECT 1 FROM " + QString(kEmbeddingDBMetaDataTable)
            + " WHERE source = '" + docFilePath + "') THEN 1 ELSE 0 END";

    {
        QMutexLocker lk(dbMtx);
        EmbedDBVendorIns->executeQuery(dataBase, query, result);
    }

    if (result.isEmpty())
        return 0;
    if (!result[0][0].isValid())
        return 0;
    return result[0][0].toBool();
}

void Embedding::embeddingClear()
{
    embedDataCache.clear();
    embedVectorCache.clear();
}

QMap<faiss::idx_t, QVector<float> > Embedding::getEmbedVectorCache()
{
    QMutexLocker lk(&embeddingMutex);
    return embedVectorCache;
}

QMap<faiss::idx_t, QPair<QString, QString>> Embedding::getEmbedDataCache()
{
    QMutexLocker lk(&embeddingMutex);
    return embedDataCache;
}

QStringList Embedding::textsSpliter(QString &texts)
{
    QStringList chunks;
    QStringList splitTexts;

    QRegularExpression regexSplit("[\n，；。,.]");
    QRegularExpression regexInvalidChar("[\\s\u200B]+");
    texts.replace(regexInvalidChar, " ");
    texts.replace("'", "\""); //SQL语句有单引号会报错
    splitTexts = texts.split(regexSplit, QtStringSkipEmptyParts);

    QString over = "";
    for (auto text : splitTexts) {
        text = over + text;
        over = "";

        if (text.length() > kMaxChunksSize) {
            textsSplitSize(text, chunks, over);
        } else if (text.length() > kMinChunksSize && text.length() < kMaxChunksSize) {
            chunks << text;
        } else {
            over = text;
        }
    }

    if (over.length() > kMinChunksSize)
        chunks << over;
    else {
        if (chunks.isEmpty())
            chunks << over;
        else
            chunks.last() += over;
    }

    return chunks;
}

void Embedding::textsSplitSize(const QString &text, QStringList &splits, QString &over, int pos)
{
    if (pos >= text.length()) {
        return;
    }

    QString part = text.mid(pos, kMaxChunksSize);

    if (part.length() < kMaxChunksSize) {
        over += part;
        return;
    }
    splits << part;
    textsSplitSize(text, splits, over, pos + kMaxChunksSize);
}

QPair<QString, QString> Embedding::getDataCacheFromID(const faiss::idx_t &id)
{
    QMutexLocker lk(&embeddingMutex);
    return QPair<QString, QString>(embedDataCache[id].first, embedDataCache[id].second);
}

QString Embedding::saveAsDocPath(const QString &doc)
{
    QString docDirStr = workerDir() + QDir::separator() + appID + QDir::separator() + "Docs";
    QDir docDir(docDirStr);
    if (!docDir.exists()) {
        if (!docDir.mkpath(docDirStr)) {
            qCWarning(logIndexCenter) << "Failed to create directory:" << docDirStr;
            return "";
        }
    }

    return docDirStr + QDir::separator() + QFileInfo(doc).fileName();
}

QString Embedding::parserDoc(const QString &path)
{
    QSharedPointer<AbstractFileParser> parser(ParserFactory::instance()->createParserForFile(path));
    if (!parser) {
        qCWarning(logIndexCenter) << "file parser create failed, don't support file:" << path;
        return QString();
    }

    QSharedPointer<ParserResult> parseResult = parser->parseFile(path);
    if (parseResult.isNull() || parseResult->status() != 0) {
        QString errorMsg = parseResult.isNull() ? "Parse result is null" : parseResult->errorString();
        qCWarning(logIndexCenter) << "Failed to parse document:" << path << "Error:" << errorMsg;

        return QString();
    }

    QString extractedText = parseResult->getAllText();
    if (extractedText.isEmpty()) {
        qCWarning(logIndexCenter) << "No text content extracted from file:" << path;
        return QString();
    }

    return extractedText;
}

void Embedding::initDataStatus()
{
    qCInfo(logIndexCenter) << "Starting data status initialization";

    QStringList dataTableFiles;
    QList<QVariantList> result;
    {
        QMutexLocker lk(dbMtx);
        QString queryDocs = "SELECT source, content FROM " + QString(kEmbeddingDBMetaDataTable);
        EmbedDBVendorIns->executeQuery(dataBase, queryDocs, result);
    }
    for (const QVariantList &res : result) {
        if (res.isEmpty())
            break;

        if (!res[0].isValid() || !res[1].isValid())
            continue;

        dataTableFiles.append(res[0].toString());
    }

    QList<QVariantList> statusResult;
    {
        // query doc status
        QMutexLocker lk(dbMtx);
        QString queryDocs = "SELECT file, status FROM " + QString(kEmbeddingDBFileStatusTable);
        EmbedDBVendorIns->executeQuery(dataBase, queryDocs, statusResult);
    }

    // status processing -> error
    QStringList statusTableFiles;
    QStringList updateStatusFiles;
    QStringList deleteStatusFiles;
    for (const QVariantList &res : statusResult) {
        if (res.isEmpty())
            break;

        if (!res[0].isValid() || !res[1].isValid())
            continue;

        QString docName = res[0].toString();
        int docStatus = res[1].toInt();

        statusTableFiles.append(docName);
        if (docStatus == EmbeddingStatus::EmbeddingStart) {
            updateStatusFiles.append(docName);
        } else if (docStatus == EmbeddingStatus::Success && !dataTableFiles.contains(docName)) {
            deleteStatusFiles.append(docName);
        }
    }
    QString updateStr = "(";
    QString deleteStr = "(";
    for (const QString &source : updateStatusFiles) {
        if (updateStatusFiles.last() == source) {
            updateStr += "'" + source + "')";
            break;
        }
        updateStr += "'" + source + "', ";
    }
    for (const QString &source : deleteStatusFiles) {
        if (deleteStatusFiles.last() == source) {
            deleteStr += "'" + source + "')";
            break;
        }
        deleteStr += "'" + source + "', ";
    }
    {
        QMutexLocker lk(dbMtx);
        QString updateStatus = "UPDATE " + QString(kEmbeddingDBFileStatusTable) + " SET status = 3 WHERE file IN " + updateStr;
        QString deleteStatus = "DELETE FROM " + QString(kEmbeddingDBFileStatusTable) + " WHERE file IN " + deleteStr;
        EmbedDBVendorIns->executeQuery(dataBase, updateStatus);
        EmbedDBVendorIns->executeQuery(dataBase, deleteStatus);
    }

    //
    QString updateStatusStr = "INSERT INTO " + QString(kEmbeddingDBFileStatusTable) + " VALUES ";
    QStringList tmpFiles;
    for (const QString &file : dataTableFiles) {
        if (!statusTableFiles.contains(file) && !tmpFiles.contains(file)) {
            QString data = "('" + file + "', 1),";
            updateStatusStr += data;

            tmpFiles.append(file);
        }
    }
    updateStatusStr.chop(1);
    if (!tmpFiles.isEmpty()) {
        QMutexLocker lk(dbMtx);
        EmbedDBVendorIns->executeQuery(dataBase, updateStatusStr);
    }
}

QString Embedding::loadTextsFromSearch(int topK, const QMap<float, faiss::idx_t> &cacheSearchRes, const QMap<float, faiss::idx_t> &dumpSearchRes)
{
    qCDebug(logIndexCenter) << "Starting search with topK:" << topK << "cache results:" << cacheSearchRes.size() << "dump results:" << dumpSearchRes.size();

    QJsonObject resultObj;
    resultObj["version"] = SEARCH_RESULT_VERSION;
    QJsonArray resultArray;

    if (appID == kSystemAssistantKey) {
        for (auto dumpIt : dumpSearchRes.keys()) {
            faiss::idx_t id = dumpSearchRes.value(dumpIt);
            QList<QVariantList> result;

            QString query = "SELECT * FROM " + QString(kEmbeddingDBMetaDataTable) + " WHERE id = " + QString::number(id);
            {
                QMutexLocker lk(dbMtx);
                EmbedDBVendorIns->executeQuery(dataBase, query, result);
            }

            if (result.isEmpty()) {
                return {};
            }

            QVariantList &res = result[0];
            if (!res[1].isValid() || !res[2].isValid())
                continue;

            QString source = res[1].toString();
            QString content = res[2].toString();
            QJsonObject obj;
            obj[kEmbeddingDBMetaDataTableSource] = source;
            obj[kEmbeddingDBMetaDataTableContent] = content;
            obj[kSearchResultDistance] = static_cast<double>(dumpIt);
            resultArray.append(obj);
        }
        resultObj["result"] = resultArray;
        return QJsonDocument(resultObj).toJson(QJsonDocument::Compact);
    }

    //TODO:重排序\合并两种结果；两个距离有序数组的合并
    int sort = 1;
    int i = 0;
    int j = 0;

    while (i < cacheSearchRes.size() && j < dumpSearchRes.size()) {
        if (sort > topK)
            break;

        auto cacheIt = cacheSearchRes.begin() + i;
        auto dumpIt = dumpSearchRes.begin() + j;
        if (cacheIt.key() < dumpIt.key()) {
            QString source = getDataCacheFromID(cacheIt.value()).first;
            QString content = getDataCacheFromID(cacheIt.value()).second;
            QJsonObject obj;
            obj[kEmbeddingDBMetaDataTableSource] = source;
            obj[kEmbeddingDBMetaDataTableContent] = content;
            obj[kSearchResultDistance] = static_cast<double>(cacheIt.key());
            resultArray.append(obj);
            sort++;
            i++;
        } else {
            faiss::idx_t id = dumpIt.value();
            QList<QVariantList> result;

            {
                QString query = "SELECT * FROM " + QString(kEmbeddingDBMetaDataTable) + " WHERE id = " + QString::number(id);
                QMutexLocker lk(dbMtx);
                EmbedDBVendorIns->executeQuery(dataBase, query, result);
            }

            if (result.isEmpty()) {
                qCDebug(logIndexCenter) << "No result found for ID:" << id;
                j++;
                continue;
            }

            QVariantList &res = result[0];
            if (!res[1].isValid() || !res[2].isValid()) {
                qCDebug(logIndexCenter) << "Invalid result data for ID:" << id;
                continue;
            }

            QString source = res[1].toString();
            QString content = res[2].toString();
            QJsonObject obj;
            obj[kEmbeddingDBMetaDataTableSource] = source;
            obj[kEmbeddingDBMetaDataTableContent] = content;
            obj[kSearchResultDistance] = static_cast<double>(dumpIt.key());
            resultArray.append(obj);
            sort++;
            j++;
        }
    }

    while (i < cacheSearchRes.size()) {
        if (sort > topK)
            break;

        auto cacheIt = cacheSearchRes.begin() + i;
        QString source = getDataCacheFromID(cacheIt.value()).first;
        QString content = getDataCacheFromID(cacheIt.value()).second;
        QJsonObject obj;
        obj[kEmbeddingDBMetaDataTableSource] = source;
        obj[kEmbeddingDBMetaDataTableContent] = content;
        obj[kSearchResultDistance] = static_cast<double>(cacheIt.key());
        resultArray.append(obj);
        sort++;
        i++;
    }

    while (j < dumpSearchRes.size()) {
        if (sort > topK)
            break;

        auto dumpIt = dumpSearchRes.begin() + j;
        faiss::idx_t id = dumpIt.value();
        QList<QVariantList> result;
        {
            QString query = "SELECT * FROM " + QString(kEmbeddingDBMetaDataTable) + " WHERE id = " + QString::number(id);
            QMutexLocker lk(dbMtx);
            EmbedDBVendorIns->executeQuery(dataBase, query, result);
        }
        if (result.isEmpty()) {
            j++;
            continue;
        }

        QVariantList &res = result[0];
        if (!res[1].isValid() || !res[2].isValid())
            continue;

        QString source = res[1].toString();
        QString content = res[2].toString();
        QJsonObject obj;
        obj[kEmbeddingDBMetaDataTableSource] = source;
        obj[kEmbeddingDBMetaDataTableContent] = content;
        obj[kSearchResultDistance] = static_cast<double>(dumpIt.key());
        resultArray.append(obj);
        sort++;
        j++;
    }
    resultObj["result"] = resultArray;
    qCInfo(logIndexCenter) << "Generated search results with" << resultArray.size() << "items";
    return QJsonDocument(resultObj).toJson(QJsonDocument::Compact);
}

void Embedding::deleteCacheIndex(const QStringList &files)
{
    if (files.isEmpty())
        return;

    QMutexLocker lk(&embeddingMutex);
    for (auto id : embedDataCache.keys()) {
        if (!files.contains(embedDataCache.value(id).first))
            continue;

        //删除缓存文档数据、删除向量
        embedDataCache.remove(id);
        embedVectorCache.remove(id);
    }
}

bool Embedding::doIndexDump(faiss::idx_t startID, faiss::idx_t endID)
{
    QMutexLocker lk(&embeddingMutex);
    //插入源信息
    QStringList insertSqlstrs;
    for (faiss::idx_t id = startID; id <= endID; id++) {
        if (!embedDataCache.contains(id))
            continue;

        QString queryStr = "INSERT INTO embedding_metadata (id, source, content) VALUES ("
                + QString::number(id) + ", '" + embedDataCache.value(id).first + "', " + "'" + embedDataCache.value(id).second + "')";
        insertSqlstrs << queryStr;

        embedDataCache.remove(id);
        embedVectorCache.remove(id);
    }

    if (insertSqlstrs.isEmpty()) {
        qCDebug(logIndexCenter) << "No data to dump for IDs" << startID << "to" << endID;
        return false;
    }

    if (!batchInsertDataToDB(insertSqlstrs)) {
        qCWarning(logIndexCenter) << "Failed to insert data into database";
        return false;
    }

    qCInfo(logIndexCenter) << "Successfully dumped" << insertSqlstrs.size() << "records to database";
    return true;
}

bool Embedding::doSaveAsDoc(const QString &file)
{
    QString newDocPath = saveAsDocPath(file);

    QProcess process;
    process.start("cp", QStringList() << file << newDocPath);
    process.waitForFinished();
    if (process.exitCode() == 0) {
        process.start("chmod", QStringList() << "444" << newDocPath);
        process.waitForFinished();
        if (process.exitCode() == 0) {
            qCInfo(logIndexCenter) << "File copied successfully:" << newDocPath;
            return true;
        } else {
            qCWarning(logIndexCenter) << "Failed to set file permissions:" << newDocPath;
            return false;
        }
    } else {
        qCWarning(logIndexCenter) << "Failed to copy file:" << file << "to" << newDocPath;
        return false;
    }
}

bool Embedding::doDeleteSaveAsDoc(const QStringList &files)
{
    for (const QString &oldDocPath : files) {
        QString docDirStr = workerDir() + QDir::separator() + appID + QDir::separator() + "Docs";
        QDir docDir(docDirStr);
        if (!docDir.exists()) {
            if (!docDir.mkpath(docDirStr)) {
                qCWarning(logIndexCenter) << "Failed to create directory:" << docDirStr;
                return false;
            }
        }
        QString newDocPath = saveAsDocPath(oldDocPath);

        QProcess process;
        process.start("rm", QStringList() << newDocPath);
        process.waitForFinished();

        if (process.exitCode() == 0) {
            qCInfo(logIndexCenter) << "Successfully deleted file:" << newDocPath;
        } else {
            qCWarning(logIndexCenter) << "Failed to delete file:" << newDocPath;
            return false;
        }
    }
    return true;
}

void Embedding::appendAbortFile(const QString &file)
{
    if (file.isEmpty())
        return;

    abortFiles.append(file);
    qCInfo(logIndexCenter) << "Added file to abort list:" << file;
}

void Embedding::setEmbeddingStatus(const QString &file, int status)
{
    QString query = "INSERT INTO " + QString(kEmbeddingDBFileStatusTable) + " (file, status) VALUES (" + "'" + file + "', "
            + QString::number(status) + ") ON CONFLICT(file) DO UPDATE SET status = " + QString::number(status);
    {
        QMutexLocker lk(dbMtx);
        EmbedDBVendorIns->executeQuery(dataBase, query);
    }
    qCDebug(logIndexCenter) << "Updated embedding status for file:" << file << "to status:" << status;
}
