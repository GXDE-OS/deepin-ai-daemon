// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexworker.h"
#include "private/indexworker_p.h"
#include "indexcenter/parser/audiopropertyparser.h"
#include "indexcenter/parser/videopropertyparser.h"
#include "indexcenter/parser/imagepropertyparser.h"
#include "config/dconfigmanager.h"

#include "analyzer/chineseanalyzer.h"

#include <QStandardPaths>
#include <QRegularExpression>
#include <QMimeDatabase>
#include <QMetaEnum>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QDir>

#include <dirent.h>

using namespace Lucene;

Q_DECLARE_LOGGING_CATEGORY(logIndexCenter)

IndexWorkerPrivate::IndexWorkerPrivate(QObject *parent)
    : QObject(parent)
{
    propertyParsers.insert("default", new AbstractPropertyParser(this));
    propertyParsers.insert("image/*", new ImagePropertyParser(this));
    propertyParsers.insert("audio/*", new AudioPropertyParser(this));
    propertyParsers.insert("video/*", new VideoPropertyParser(this));
}

bool IndexWorkerPrivate::indexExists()
{
    return IndexReader::indexExists(FSDirectory::open(indexStoragePath().toStdWString()));
}

bool IndexWorkerPrivate::isFilter(const QString &file)
{
    static const auto &blacklist = ConfigManagerIns->value(BLACKLIST_GROUP, BLACKLIST_PATHS, QStringList()).toStringList();
    return std::any_of(blacklist.begin(), blacklist.end(), [&file](const QString &path) {
        return file.startsWith(path);
    });
}

Lucene::IndexWriterPtr IndexWorkerPrivate::newIndexWriter(bool create)
{
    return newLucene<IndexWriter>(FSDirectory::open(indexStoragePath().toStdWString()),
                                  newLucene<ChineseAnalyzer>(),
                                  create,
                                  IndexWriter::MaxFieldLengthLIMITED);
}

Lucene::IndexReaderPtr IndexWorkerPrivate::newIndexReader()
{
    return IndexReader::open(FSDirectory::open(indexStoragePath().toStdWString()), true);
}

void IndexWorkerPrivate::doIndexTask(const Lucene::IndexWriterPtr &writer, const QString &file, IndexWorkerPrivate::IndexType type, bool isCheck)
{
    if (isStoped || isFilter(file))
        return;

    // limit file name length and level
    if (file.size() > FILENAME_MAX - 1 || file.count('/') > 20)
        return;

    const std::string tmp = file.toStdString();
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
        while ((dent = readdir(dir)) && !isStoped) {
            if (dent->d_name[0] == '.')
                continue;

            if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, ".."))
                continue;

            strncpy(fn + len, dent->d_name, FILENAME_MAX - len);
            doIndexTask(writer, fn, type, isCheck);
        }

        if (dir)
            closedir(dir);
        return;
    }

    if (isCheck && !checkUpdate(writer->getReader(), file, type))
        return;
    indexFile(writer, file, type);
}

void IndexWorkerPrivate::indexFile(Lucene::IndexWriterPtr writer, const QString &file, IndexWorkerPrivate::IndexType type)
{
    Q_ASSERT(writer);
    indexFileCount++;
    try {
        switch (type) {
        case CreateIndex: {
            // 添加
            if (auto doc = indexDocument(file)) {
                qCDebug(logIndexCenter) << "Adding file to index:" << file;
                writer->addDocument(doc);
            }
            break;
        }
        case UpdateIndex: {
            // 更新
            if (auto doc = indexDocument(file)) {
                qCDebug(logIndexCenter) << "Updating file in index:" << file;
                TermPtr term = newLucene<Term>(L"path", file.toStdWString());
                writer->updateDocument(term, doc);
            }
            break;
        }
        }
    } catch (const LuceneException &e) {
        QMetaEnum enumType = QMetaEnum::fromType<IndexWorkerPrivate::IndexType>();
        qCWarning(logIndexCenter) << "Lucene exception during indexing - Error:" << QString::fromStdWString(e.getError()) 
                                 << "Type:" << enumType.valueToKey(type) << "File:" << file;
    } catch (const std::exception &e) {
        QMetaEnum enumType = QMetaEnum::fromType<IndexWorkerPrivate::IndexType>();
        qCWarning(logIndexCenter) << "Exception during indexing - Error:" << QString(e.what()) 
                                 << "Type:" << enumType.valueToKey(type) << "File:" << file;
    } catch (...) {
        qCWarning(logIndexCenter) << "Unknown error during indexing - File:" << file;
    }
}

bool IndexWorkerPrivate::checkUpdate(const IndexReaderPtr &reader, const QString &file, IndexWorkerPrivate::IndexType &type)
{
    Q_ASSERT(reader);

    try {
        SearcherPtr searcher = newLucene<IndexSearcher>(reader);
        TermQueryPtr query = newLucene<TermQuery>(newLucene<Term>(L"path", file.toStdWString()));

        // 文件路径为唯一值，所以搜索一个结果就行了
        TopDocsPtr topDocs = searcher->search(query, 1);
        int32_t numTotalHits = topDocs->totalHits;
        if (numTotalHits == 0) {
            type = CreateIndex;
            return true;
        } else {
            DocumentPtr doc = searcher->doc(topDocs->scoreDocs[0]->doc);
            QFileInfo info(file);
            QString lastModified = info.lastModified().toString("yyyyMMddHHmmss");
            String storeTime = doc->get(L"lastModified");

            if (lastModified.toStdWString() != storeTime) {
                type = UpdateIndex;
                return true;
            }
        }
    } catch (const LuceneException &e) {
        qCWarning(logIndexCenter) << "Lucene exception during file check - Error:" << QString::fromStdWString(e.getError())
                                 << "File:" << file;
    } catch (const std::exception &e) {
        qCWarning(logIndexCenter) << "Exception during file check - Error:" << QString(e.what()) 
                                 << "File:" << file;
    } catch (...) {
        qCWarning(logIndexCenter) << "Unknown error during file check - File:" << file;
    }

    return false;
}

Lucene::DocumentPtr IndexWorkerPrivate::indexDocument(const QString &file)
{
    const auto &properties = fileProperties(file);
    if (properties.isEmpty())
        return {};

    DocumentPtr doc = newLucene<Document>();
    for (const auto &iter : properties) {
        doc->add(newLucene<Field>(iter.field.toStdWString(),
                                  iter.contents.toStdWString(),
                                  Field::STORE_YES,
                                  iter.analyzed ? Field::INDEX_ANALYZED : Field::INDEX_NOT_ANALYZED));
    }

    return doc;
}

QList<AbstractPropertyParser::Property> IndexWorkerPrivate::fileProperties(const QString &file)
{
    static QMimeDatabase database;

    QList<AbstractPropertyParser::Property> properties;
    const auto &type = database.mimeTypeForFile(file);
    const auto &mimeName = type.name();
    for (const auto &mimeRegx : propertyParsers.keys()) {
        QRegularExpression regx(mimeRegx);
        if (mimeName.contains(regx)) {
            properties = propertyParsers.value(mimeRegx)->properties(file);
            break;
        }
    }

    // 优化性能：特征库不为普通文件创建索引
//    if (properties.isEmpty())
//        properties = propertyParsers.value("default")->properties(file);

    return properties;
}

IndexWorker::IndexWorker(QObject *parent)
    : QObject(parent),
      d(new IndexWorkerPrivate(this))
{
}

void IndexWorker::start()
{
    d->isStoped = false;
}

void IndexWorker::stop()
{
    d->isStoped = true;
}

void IndexWorker::onFileAttributeChanged(const QString &file)
{
    if (d->isStoped)
        return;

    try {
        IndexWriterPtr writer = d->newIndexWriter();
        d->doIndexTask(writer, file, IndexWorkerPrivate::UpdateIndex);
        writer->optimize();
        writer->close();
    } catch (const LuceneException &e) {
        qCWarning(logIndexCenter) << "Lucene exception during file attribute update - Error:" << QString::fromStdWString(e.getError()) 
                                 << "File:" << file;
    } catch (const std::exception &e) {
        qCWarning(logIndexCenter) << "Exception during file attribute update - Error:" << QString(e.what()) 
                                 << "File:" << file;
    } catch (...) {
        qCWarning(logIndexCenter) << "Unknown error during file attribute update - File:" << file;
    }
}

void IndexWorker::onFileCreated(const QString &file)
{
    if (d->isStoped)
        return;

    QDir dir;
    if (!dir.exists(d->indexStoragePath())) {
        if (!dir.mkpath(d->indexStoragePath())) {
            qCWarning(logIndexCenter) << "Failed to create index directory - Path:" << d->indexStoragePath();
            return;
        }
    }

    try {
        // record spending
        QElapsedTimer timer;
        timer.start();
        d->indexFileCount = 0;
        IndexWriterPtr writer = d->newIndexWriter(!d->indexExists());
        d->doIndexTask(writer, file, IndexWorkerPrivate::CreateIndex);
        writer->optimize();
        writer->close();

        qCInfo(logIndexCenter) << "Index creation completed - Time:" << timer.elapsed() << "ms Files:" << d->indexFileCount;
    } catch (const LuceneException &e) {
        qCWarning(logIndexCenter) << "Lucene exception during file creation - Error:" << QString::fromStdWString(e.getError()) 
                                 << "File:" << file;
    } catch (const std::exception &e) {
        qCWarning(logIndexCenter) << "Exception during file creation - Error:" << QString(e.what())
                                 << "File:" << file;
    } catch (...) {
        qCWarning(logIndexCenter) << "Unknown error during file index creation - File:" << file;
    }
}

void IndexWorker::onFileDeleted(const QString &file)
{
    if (d->isStoped)
        return;

    try {
        qCDebug(logIndexCenter) << "Deleting file from index:" << file;
        IndexWriterPtr writer = d->newIndexWriter();

        QFileInfo info(file);
        if (info.isDir()) {
            TermPtr term = newLucene<Term>(L"path", (file + "/*").toStdWString());
            QueryPtr query = newLucene<WildcardQuery>(term);
            writer->deleteDocuments(query);
        } else {
            TermPtr term = newLucene<Term>(L"path", file.toStdWString());
            writer->deleteDocuments(term);
        }

        writer->optimize();
        writer->close();
    } catch (const LuceneException &e) {
        qCWarning(logIndexCenter) << "Lucene exception during file deletion - Error:" << QString::fromStdWString(e.getError());
    } catch (const std::exception &e) {
        qCWarning(logIndexCenter) << "Exception during file deletion - Error:" << QString(e.what());
    } catch (...) {
        qCWarning(logIndexCenter) << "Unknown error during file index deletion";
    }
}

void IndexWorker::onCreateAllIndex()
{
    ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED, false);
    if (d->isStoped)
        return;

    if (d->indexExists()) {
        QTimer::singleShot(10 * 1000, this, &IndexWorker::onUpdateAllIndex);
        return;
    }

    qCInfo(logIndexCenter) << "Starting full index creation";
    onFileCreated(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED, true);
    ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED_TIME_S, QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    qCInfo(logIndexCenter) << "Full index creation completed";
}

void IndexWorker::onUpdateAllIndex()
{
    if (d->isStoped)
        return;

    qCInfo(logIndexCenter) << "Starting full index update";
    const auto &path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    try {
        IndexWriterPtr writer = d->newIndexWriter();
        d->doIndexTask(writer, path, IndexWorkerPrivate::UpdateIndex, true);
        writer->optimize();
        writer->close();
    } catch (const LuceneException &e) {
        qCWarning(logIndexCenter) << "Lucene exception during full update - Error:" << QString::fromStdWString(e.getError());
    } catch (const std::exception &e) {
        qCWarning(logIndexCenter) << "Exception during full update - Error:" << QString(e.what());
    } catch (...) {
        qCWarning(logIndexCenter) << "Unknown error during full index update";
    }
    ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED, true);
    ConfigManagerIns->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED_TIME_S, QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    qCInfo(logIndexCenter) << "Full index update completed";
}
