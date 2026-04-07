// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DCONFIGMANAGER_H
#define DCONFIGMANAGER_H

#include <aidaemon_global.h>

#include <QObject>
#include <QVariant>

AIDAEMON_BEGIN_NAMESPACE

#define APP_GROUP "deepin-ai-daemon"
#define ISFIRSTDCONFIG "isFirstDconfig"

#define BLACKLIST_GROUP "deepin-ai-daemon.blacklist"
#define BLACKLIST_PATHS "Paths"

#define ENABLE_EMBEDDING_FILES_LIST_GROUP "deepin-ai-daemon.enableembeddingfileslist"
#define ENABLE_EMBEDDING_PATHS "Paths"

#define AUTO_INDEX_GROUP "deepin-ai-daemon.autoindex"
#define AUTO_INDEX_STATUS "Status"

#define SEMANTIC_ANALYSIS_GROUP "deepin-ai-daemon.semanticanalysis"
#define ENABLE_SEMANTIC_ANALYSIS "EnableSemanticAnalysis"
#define SEMANTIC_ANALYSIS_FINISHED "SemanticAnalysisFinished"
#define SEMANTIC_ANALYSIS_FINISHED_TIME_S "SemanticAnalysisFinishedTimeS"

#define ConfigManagerIns AIDAEMON_NAMESPACE::DConfigManager::instance()

// 默认值路径 /usr/share/dsg/configs/
// 本地缓存路径 ~/.config/dsg/configs/

class DConfigManagerPrivate;
class DConfigManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(DConfigManager)

public:
    static DConfigManager *instance();

    bool addConfig(const QString &config, QString *err = nullptr);
    bool removeConfig(const QString &config, QString *err = nullptr);

    QStringList keys(const QString &config) const;
    bool contains(const QString &config, const QString &key) const;
    QVariant value(const QString &config, const QString &key, const QVariant &fallback = QVariant()) const;
    void setValue(const QString &config, const QString &key, const QVariant &value);

    bool validateConfigs(QStringList &invalidConfigs) const;

    void configMigrate();

Q_SIGNALS:
    void valueChanged(const QString &config, const QString &key);

private:
    explicit DConfigManager(QObject *parent = nullptr);
    virtual ~DConfigManager() override;

private:
    QScopedPointer<DConfigManagerPrivate> d;
};

AIDAEMON_END_NAMESPACE

#endif   // DCONFIGMANAGER_H
