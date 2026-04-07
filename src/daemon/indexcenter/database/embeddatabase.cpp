// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddatabase.h"
#include <QLoggingCategory>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(logIndexCenter)

EmbedDBVendor *EmbedDBVendor::instance()
{
    static EmbedDBVendor ins;
    return &ins;
}

QSqlDatabase EmbedDBVendor::addDatabase(const QString &databasePath)
{
    //打开数据库
    //QString databasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QDir::separator() +  databaseName;
    auto db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    db.setDatabaseName(databasePath);
    return db;
}

void EmbedDBVendor::removeDatabase(QSqlDatabase *db)
{
    if (db)
        QSqlDatabase::removeDatabase(db->connectionName());
}

bool EmbedDBVendor::executeQuery(QSqlDatabase *db, const QString &queryStr, QList<QVariantList> &result)
{
    bool ret = false;

    if (!openDB(db))
        return ret;

    QSqlQuery query(*db);
    if (query.exec(queryStr)) {
        while (query.next()) {
            QVariantList res;
            res.append(query.value(0));
            res.append(query.value(1));
            res.append(query.value(2));

            result.append(res);
        }
        ret = true;
    } else {
        qCWarning(logIndexCenter) << "Failed to execute query:" << query.lastError().text() 
                                 << "Query:" << queryStr;
    }

    closeDB(db);
    return ret;
}

bool EmbedDBVendor::executeQuery(QSqlDatabase *db, const QString &queryStr)
{
    bool ret = false;

    if (!openDB(db))
        return ret;

    QSqlQuery query(*db);
    if (query.exec(queryStr)) {
        ret = true;
    } else {
        qCWarning(logIndexCenter) << "Failed to execute query:" << query.lastError().text() 
                                 << "Query:" << queryStr;
    }

    closeDB(db);
    return ret;
}

bool EmbedDBVendor::commitTransaction(QSqlDatabase *db, const QStringList &queryList)
{
    bool ret = true;

    if (!openDB(db))
        return false;

    QSqlQuery query(*db);
    if (query.exec("BEGIN TRANSACTION")) {
        for (const QString &queryStr : queryList) {
            if (!query.exec(queryStr)) {
                qCWarning(logIndexCenter) << "Failed to execute query in transaction:" 
                                        << query.lastError().text() 
                                        << "Query:" << queryStr;
                ret = false;
                break;
            }
        }

        if (ret) {
            if (!query.exec("COMMIT")) {
                qCWarning(logIndexCenter) << "Failed to commit transaction for database:" 
                                        << db->databaseName();
                ret = false;
            }
        }
    } else {
        qCWarning(logIndexCenter) << "Failed to begin transaction for database:" 
                                << db->databaseName();
        ret = false;
    }

    closeDB(db);
    return ret;
}

bool EmbedDBVendor::isEmbedDataTableExists(QSqlDatabase *db, const QString &tableName)
{
    bool ret = false;

    if (!openDB(db))
        return ret;

    QSqlQuery query(*db);
    query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=:tableName");
    query.bindValue(":tableName", tableName);
    ret = query.exec();
    if (ret) {
        ret = query.next();
    } else {
        qCWarning(logIndexCenter) << "Failed to check table existence:" 
                                << query.lastError().text() 
                                << "Table:" << tableName;
    }

    closeDB(db);
    return ret;
}

bool EmbedDBVendor::openDB(QSqlDatabase *db)
{
    if (!(db->isOpen() || db->open())) {
        qCWarning(logIndexCenter) << "Failed to open database:" << db->databaseName();
        return false;
    }
    return true;
}

void EmbedDBVendor::closeDB(QSqlDatabase *db)
{
    db->close();
}

EmbedDBVendor::EmbedDBVendor()
{

}
