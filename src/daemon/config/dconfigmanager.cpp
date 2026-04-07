// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dconfigmanager.h"
#include "private/dconfigmanager_p.h"

#include <DConfig>

#include <QDebug>
#include <QSet>
#include <QStringList>
#include <QStandardPaths>
#include <QSettings>
#include <QGuiApplication>
#include <QFileInfo>
#include <QLoggingCategory>

static constexpr char kCfgAppId[] { "deepin-ai-daemon" };

AIDAEMON_USE_NAMESPACE
DCORE_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logConfig)

DConfigManager::DConfigManager(QObject *parent)
    : QObject(parent), d(new DConfigManagerPrivate(this))
{
    addConfig(BLACKLIST_GROUP);
    addConfig(ENABLE_EMBEDDING_FILES_LIST_GROUP);
    addConfig(AUTO_INDEX_GROUP);
    addConfig(SEMANTIC_ANALYSIS_GROUP);

    this->configMigrate();

    if (this->value(BLACKLIST_GROUP, BLACKLIST_PATHS, QStringList()).toStringList().empty()) {
        QStringList blacklist {
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/WXWork",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/WeChat Files",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Tencent Files",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/xwechat_files"
        };
        this->setValue(BLACKLIST_GROUP, BLACKLIST_PATHS, blacklist);
    }
    if (this->value(ENABLE_EMBEDDING_FILES_LIST_GROUP, ENABLE_EMBEDDING_PATHS, QStringList()).toStringList().empty()) {
        //添加默认的可Embedding的文件夹
        QStringList enableEmbeddingFiles {
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Embedding",
        };
        this->setValue(ENABLE_EMBEDDING_FILES_LIST_GROUP, ENABLE_EMBEDDING_PATHS, enableEmbeddingFiles);
    }
}

DConfigManager *DConfigManager::instance()
{
    static DConfigManager ins;
    return &ins;
}

DConfigManager::~DConfigManager()
{
#ifdef DTKCORE_CLASS_DConfig
    QWriteLocker locker(&d->lock);

    auto configs = d->configs.values();
    std::for_each(configs.begin(), configs.end(), [](DConfig *cfg) { delete cfg; });
    d->configs.clear();
#endif
}

bool DConfigManager::addConfig(const QString &config, QString *err)
{
#ifdef DTKCORE_CLASS_DConfig
    QWriteLocker locker(&d->lock);

    if (d->configs.contains(config)) {
        if (err)
            *err = "config is already added";
        qCWarning(logConfig) << "Failed to add config:" << config << "- Config already exists";
        return false;
    }

    auto cfg = DConfig::create(kCfgAppId, config, "", this);
    if (!cfg) {
        if (err)
            *err = "cannot create config";
        qCWarning(logConfig) << "Failed to create config:" << config;
        return false;
    }

    if (!cfg->isValid()) {
        if (err)
            *err = "config is not valid";
        qCWarning(logConfig) << "Invalid config:" << config;
        delete cfg;
        return false;
    }

    d->configs.insert(config, cfg);
    locker.unlock();
    connect(cfg, &DConfig::valueChanged, this, [=](const QString &key) { Q_EMIT valueChanged(config, key); });
#endif
    return true;
}

bool DConfigManager::removeConfig(const QString &config, QString *err)
{
#ifdef DTKCORE_CLASS_DConfig
    QWriteLocker locker(&d->lock);

    if (d->configs.contains(config)) {
        delete d->configs[config];
        d->configs.remove(config);
    }
#endif
    return true;
}

QStringList DConfigManager::keys(const QString &config) const
{
#ifdef DTKCORE_CLASS_DConfig
    QReadLocker locker(&d->lock);

    if (!d->configs.contains(config))
        return QStringList();

    return d->configs[config]->keyList();
#else
    return QStringList();
#endif
}

bool DConfigManager::contains(const QString &config, const QString &key) const
{
    return key.isEmpty() ? false : keys(config).contains(key);
}

QVariant DConfigManager::value(const QString &config, const QString &key, const QVariant &fallback) const
{
#ifdef DTKCORE_CLASS_DConfig
    QReadLocker locker(&d->lock);

    if (d->configs.contains(config)) {
        //qDebug() << "dconfig:" << config << "key:" << key << "value:" << d->configs.value(config)->value(key, fallback);
        return d->configs.value(config)->value(key, fallback);
    }
    else
        qWarning() << "Config: " << config << "is not registered!!!";
    return fallback;
#else
    return fallback;
#endif
}

void DConfigManager::setValue(const QString &config, const QString &key, const QVariant &value)
{
#ifdef DTKCORE_CLASS_DConfig
    QReadLocker locker(&d->lock);

    if (d->configs.contains(config)) {
        qCDebug(logConfig) << "Setting config value - Config:" << config << "Key:" << key << "Value:" << value;
        d->configs.value(config)->setValue(key, value);
    } else {
        qCWarning(logConfig) << "Failed to set value - Config not found:" << config;
    }
#endif
}

bool DConfigManager::validateConfigs(QStringList &invalidConfigs) const
{
#ifdef DTKCORE_CLASS_DConfig
    QReadLocker locker(&d->lock);

    bool ret = true;
    for (auto iter = d->configs.cbegin(); iter != d->configs.cend(); ++iter) {
        bool valid = iter.value()->isValid();
        if (!valid)
            invalidConfigs << iter.key();
        ret &= valid;
    }
    return ret;
#else
    return true;
#endif
}

void DConfigManager::configMigrate()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    configPath = configPath
            + "/" + qApp->organizationName()
            + "/" + qApp->applicationName()
            + "/" + qApp->applicationName() + ".conf";

    QFileInfo configFile(configPath);
    if (!configFile.exists()) {
        qCInfo(logConfig) << "No existing config file found at:" << configPath;
        return;
    }

    QSettings set(configPath, QSettings::IniFormat);
    bool isFirstDconfig = false;
    set.beginGroup("dconfig");
    isFirstDconfig = set.value(ISFIRSTDCONFIG, true).toBool();
    if (isFirstDconfig) {
        set.setValue(ISFIRSTDCONFIG, false);
    }
    set.endGroup();
    if (!isFirstDconfig) {
        qCInfo(logConfig) << "Config migration not needed - already migrated";
        return;
    }

    qCInfo(logConfig) << "Starting config migration from:" << configPath;

    set.beginGroup("BlackList");
    this->setValue(BLACKLIST_GROUP, BLACKLIST_PATHS, set.value(BLACKLIST_PATHS, QStringList()).toStringList());
    set.endGroup();

    set.beginGroup("EnableEmbeddingFilesList");
    this->setValue(ENABLE_EMBEDDING_FILES_LIST_GROUP, ENABLE_EMBEDDING_PATHS, set.value(ENABLE_EMBEDDING_PATHS, QStringList()).toStringList());
    set.endGroup();

    set.beginGroup("AutoIndex");
    for (const QString &key : set.childKeys()) {
        this->setValue(AUTO_INDEX_GROUP, key, set.value(key, false).toBool());
    }
    set.endGroup();

    set.beginGroup("SemanticAnalysis");
    this->setValue(SEMANTIC_ANALYSIS_GROUP, ENABLE_SEMANTIC_ANALYSIS, set.value(ENABLE_SEMANTIC_ANALYSIS, false).toBool());
    this->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED, set.value(SEMANTIC_ANALYSIS_FINISHED, false).toBool());
    this->setValue(SEMANTIC_ANALYSIS_GROUP, SEMANTIC_ANALYSIS_FINISHED_TIME_S, set.value(SEMANTIC_ANALYSIS_FINISHED_TIME_S, 0).toLongLong());
    set.endGroup();
    qCInfo(logConfig) << "Config migration completed successfully";
}
