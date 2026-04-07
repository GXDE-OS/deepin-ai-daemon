// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "serviceconfig.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QDir>
#include <QStorageInfo>
#include <QProcess>

AIDAEMON_VECTORDB_USE_NAMESPACE

Q_LOGGING_CATEGORY(serviceConfig, "vectordb.service.config")

ServiceConfig *ServiceConfig::s_instance = nullptr;
QMutex ServiceConfig::s_mutex;

ServiceConfig* ServiceConfig::instance()
{
    QMutexLocker locker(&s_mutex);
    if (!s_instance) {
        s_instance = new ServiceConfig();
    }
    return s_instance;
}

void ServiceConfig::destroyInstance()
{
    QMutexLocker locker(&s_mutex);
    delete s_instance;
    s_instance = nullptr;
}

ServiceConfig::ServiceConfig(QObject *parent)
    : QObject(parent)
    , m_settings(nullptr)
    , m_resourceMonitorTimer(new QTimer(this))
{
    qCDebug(serviceConfig) << "ServiceConfig created";
    
    // Initialize settings
    m_settings = new QSettings(getConfigFilePath(), QSettings::IniFormat, this);
    
    // Initialize defaults and load configuration
    initializeDefaults();
    loadConfig();
    
    // Setup resource monitoring
    m_resourceMonitorTimer->setInterval(30000); // 30 seconds
    connect(m_resourceMonitorTimer, &QTimer::timeout, this, &ServiceConfig::updateResourceUsage);
    m_resourceMonitorTimer->start();
    
    qCDebug(serviceConfig) << "ServiceConfig initialized";
}

ServiceConfig::~ServiceConfig()
{
    qCDebug(serviceConfig) << "ServiceConfig destroyed";
    saveConfig();
}

bool ServiceConfig::setConfig(const QString &key, const QVariant &value)
{
    qCDebug(serviceConfig) << "Setting config:" << key << "=" << value;
    
    if (!isValidConfigKey(key) || !validateConfigValue(key, value)) {
        qCWarning(serviceConfig) << "Invalid config key or value:" << key << value;
        return false;
    }
    
    QVariant oldValue;
    bool changed = false;
    
    {
        QMutexLocker locker(&m_configMutex);
        oldValue = m_runtimeConfig.value(key);
        if (oldValue != value) {
            m_runtimeConfig[key] = value;
            changed = true;
        }
    }
    
    if (changed) {
        notifyConfigChange(key, value);
    }
    
    return true;
}

QVariant ServiceConfig::getConfig(const QString &key, const QVariant &defaultValue) const
{
    {
        QMutexLocker locker(&m_configMutex);
        if (m_runtimeConfig.contains(key)) {
            return m_runtimeConfig[key];
        }
    }
    
    QVariant settingsValue = m_settings->value(key);
    if (settingsValue.isValid()) {
        return settingsValue;
    }
    
    QVariant defaultVal = getDefaultValue(key);
    if (defaultVal.isValid()) {
        return defaultVal;
    }
    
    return defaultValue;
}

QVariantMap ServiceConfig::getAllConfigs() const
{
    QVariantMap allConfigs;
    
    // Start with defaults
    QStringList allKeys = m_settings->allKeys();
    for (const QString &key : allKeys) {
        allConfigs[key] = getConfig(key);
    }
    
    // Override with runtime configs
    for (auto it = m_runtimeConfig.begin(); it != m_runtimeConfig.end(); ++it) {
        allConfigs[it.key()] = it.value();
    }
    
    return allConfigs;
}

bool ServiceConfig::resetToDefaults()
{
    qCDebug(serviceConfig) << "Resetting to defaults";
    
    m_runtimeConfig.clear();
    m_settings->clear();
    initializeDefaults();
    
    emit configChanged("*", QVariant()); // Notify all configs changed
    return true;
}

bool ServiceConfig::setResourceLimit(const QString &resource, const QVariant &limit)
{
    qCDebug(serviceConfig) << "Setting resource limit:" << resource << "=" << limit;
    
    if (!validateResourceType(resource)) {
        qCWarning(serviceConfig) << "Invalid resource type:" << resource;
        return false;
    }
    
    QVariant oldLimit = m_resourceLimits.value(resource);
    if (oldLimit != limit) {
        m_resourceLimits[resource] = limit;
        emit resourceLimitChanged(resource, limit);
    }
    
    return true;
}

QVariant ServiceConfig::getResourceLimit(const QString &resource) const
{
    if (m_resourceLimits.contains(resource)) {
        return m_resourceLimits[resource];
    }
    
    // Return default limits based on resource type
    if (resource == "memory") {
        return DefaultValues::MAX_MEMORY_MB;
    } else if (resource == "cpu") {
        return DefaultValues::MAX_CPU_PERCENT;
    } else if (resource == "disk") {
        return DefaultValues::MAX_DISK_GB;
    }
    
    return QVariant();
}

bool ServiceConfig::isResourceAvailable(const QString &resource, const QVariant &required) const
{
    QVariant limit = getResourceLimit(resource);
    QVariant current = m_resourceUsage.value(resource, 0);
    
    if (!limit.isValid() || !required.isValid()) {
        return false;
    }
    
    return (current.toReal() + required.toReal()) <= limit.toReal();
}

bool ServiceConfig::setDefaultModel(const QString &modelType, const QString &modelId)
{
    qCDebug(serviceConfig) << "Setting default model:" << modelType << "=" << modelId;
    
    if (!validateModelType(modelType)) {
        qCWarning(serviceConfig) << "Invalid model type:" << modelType;
        return false;
    }
    
    m_defaultModels[modelType] = modelId;
    return true;
}

QString ServiceConfig::getDefaultModel(const QString &modelType) const
{
    return m_defaultModels.value(modelType).toString();
}

QStringList ServiceConfig::getAvailableModels(const QString &modelType) const
{
    return m_availableModels.value(modelType).toStringList();
}

QVariantMap ServiceConfig::getSystemStatus() const
{
    QVariantMap status;
    
    status["isHealthy"] = m_isSystemHealthy;
    status["memoryUsage"] = getCurrentResourceUsage("memory");
    status["cpuUsage"] = getCurrentResourceUsage("cpu");
    status["diskUsage"] = getCurrentResourceUsage("disk");
    status["memoryUsagePercent"] = getResourceUsagePercentage("memory");
    status["cpuUsagePercent"] = getResourceUsagePercentage("cpu");
    status["diskUsagePercent"] = getResourceUsagePercentage("disk");
    
    return status;
}

bool ServiceConfig::isSystemHealthy() const
{
    return m_isSystemHealthy;
}

bool ServiceConfig::saveConfig()
{
    qCDebug(serviceConfig) << "Saving configuration";
    
    // Save runtime configs to settings
    for (auto it = m_runtimeConfig.begin(); it != m_runtimeConfig.end(); ++it) {
        m_settings->setValue(it.key(), it.value());
    }
    
    // Save resource limits
    for (auto it = m_resourceLimits.begin(); it != m_resourceLimits.end(); ++it) {
        m_settings->setValue(QString("resources/%1").arg(it.key()), it.value());
    }
    
    // Save default models
    for (auto it = m_defaultModels.begin(); it != m_defaultModels.end(); ++it) {
        m_settings->setValue(QString("models/default_%1").arg(it.key()), it.value());
    }
    
    m_settings->sync();
    return m_settings->status() == QSettings::NoError;
}

bool ServiceConfig::loadConfig()
{
    qCDebug(serviceConfig) << "Loading configuration";
    
    // Load resource limits
    m_settings->beginGroup("resources");
    QStringList resourceKeys = m_settings->childKeys();
    for (const QString &key : resourceKeys) {
        m_resourceLimits[key] = m_settings->value(key);
    }
    m_settings->endGroup();
    
    // Load default models
    m_settings->beginGroup("models");
    QStringList modelKeys = m_settings->childKeys();
    for (const QString &key : modelKeys) {
        if (key.startsWith("default_")) {
            QString modelType = key.mid(8); // Remove "default_" prefix
            m_defaultModels[modelType] = m_settings->value(key);
        }
    }
    m_settings->endGroup();
    
    return true;
}

qreal ServiceConfig::getCurrentResourceUsage(const QString &resource) const
{
    return m_resourceUsage.value(resource, 0.0).toReal();
}

qreal ServiceConfig::getResourceUsagePercentage(const QString &resource) const
{
    qreal usage = getCurrentResourceUsage(resource);
    qreal limit = getResourceLimit(resource).toReal();
    
    if (limit <= 0) {
        return 0.0;
    }
    
    return (usage / limit) * 100.0;
}

void ServiceConfig::onResourceUsageChanged(const QString &resource, qreal usage)
{
    qreal oldUsage = m_resourceUsage.value(resource, 0.0).toReal();
    if (qAbs(oldUsage - usage) > 0.01) { // Only update if significant change
        m_resourceUsage[resource] = usage;
        qreal percentage = getResourceUsagePercentage(resource);
        emit resourceUsageChanged(resource, usage, percentage);
    }
}

void ServiceConfig::onSystemStatusChanged()
{
    bool wasHealthy = m_isSystemHealthy;
    checkSystemHealth();
    
    if (wasHealthy != m_isSystemHealthy) {
        emit systemHealthChanged(m_isSystemHealthy);
    }
}

void ServiceConfig::initializeDefaults()
{
    // Initialize default resource limits
    m_resourceLimits["memory"] = DefaultValues::MAX_MEMORY_MB;
    m_resourceLimits["cpu"] = DefaultValues::MAX_CPU_PERCENT;
    m_resourceLimits["disk"] = DefaultValues::MAX_DISK_GB;
    
    // Initialize default models
    m_defaultModels["embedding"] = "BAAI-bge-large-zh-v1.5";
    m_defaultModels["parser"] = "default-parser";
    m_defaultModels["tts"] = "default-tts";
    
    // Initialize available models
    m_availableModels["embedding"] = QStringList{"BAAI-bge-large-zh-v1.5", "text-embedding-ada-002"};
    m_availableModels["parser"] = QStringList{"default-parser", "tesseract"};
    m_availableModels["tts"] = QStringList{"default-tts", "espeak"};
}

bool ServiceConfig::validateConfigValue(const QString &key, const QVariant &value) const
{
    Q_UNUSED(key)
    return value.isValid();
}

bool ServiceConfig::validateResourceType(const QString &resource) const
{
    static const QStringList validResources = {"memory", "cpu", "disk", "network", "gpu"};
    return validResources.contains(resource);
}

bool ServiceConfig::validateModelType(const QString &modelType) const
{
    static const QStringList validModelTypes = {"embedding", "parser", "tts", "llm"};
    return validModelTypes.contains(modelType);
}

void ServiceConfig::updateResourceUsage()
{
    onResourceUsageChanged("memory", calculateMemoryUsage());
    onResourceUsageChanged("cpu", calculateCpuUsage());
    onResourceUsageChanged("disk", calculateDiskUsage());
    
    onSystemStatusChanged();
}

void ServiceConfig::checkSystemHealth()
{
    bool healthy = true;
    
    // Check memory usage
    if (getResourceUsagePercentage("memory") > 90.0) {
        healthy = false;
    }
    
    // Check CPU usage
    if (getResourceUsagePercentage("cpu") > 95.0) {
        healthy = false;
    }
    
    // Check disk usage
    if (getResourceUsagePercentage("disk") > 95.0) {
        healthy = false;
    }
    
    m_isSystemHealthy = healthy;
}

qreal ServiceConfig::calculateMemoryUsage() const
{
    // TODO: Implement actual memory usage calculation
    return 0.0;
}

qreal ServiceConfig::calculateCpuUsage() const
{
    // TODO: Implement actual CPU usage calculation
    return 0.0;
}

qreal ServiceConfig::calculateDiskUsage() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QStorageInfo storage(dataPath);
    
    if (storage.isValid()) {
        qint64 totalBytes = storage.bytesTotal();
        qint64 availableBytes = storage.bytesAvailable();
        qint64 usedBytes = totalBytes - availableBytes;
        
        return static_cast<qreal>(usedBytes) / (1024.0 * 1024.0 * 1024.0); // Convert to GB
    }
    
    return 0.0;
}

QString ServiceConfig::getConfigFilePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    return QString("%1/deepin-ai-daemon-vectordb.conf").arg(configDir);
}

QVariant ServiceConfig::getDefaultValue(const QString &key) const
{
    // Return default values for known keys
    if (key == ConfigKeys::MAX_SESSIONS) return DefaultValues::MAX_SESSIONS;
    if (key == ConfigKeys::MAX_MEMORY_MB) return DefaultValues::MAX_MEMORY_MB;
    if (key == ConfigKeys::MAX_CPU_PERCENT) return DefaultValues::MAX_CPU_PERCENT;
    if (key == ConfigKeys::MAX_DISK_GB) return DefaultValues::MAX_DISK_GB;
    if (key == ConfigKeys::MAX_CONCURRENT_TASKS) return DefaultValues::MAX_CONCURRENT_TASKS;
    if (key == ConfigKeys::THREAD_POOL_SIZE) return DefaultValues::THREAD_POOL_SIZE;
    if (key == ConfigKeys::CACHE_SIZE_MB) return DefaultValues::CACHE_SIZE_MB;
    if (key == ConfigKeys::INDEX_CACHE_ENABLED) return DefaultValues::INDEX_CACHE_ENABLED;
    if (key == ConfigKeys::BATCH_PROCESSING_SIZE) return DefaultValues::BATCH_PROCESSING_SIZE;
    if (key == ConfigKeys::SESSION_TIMEOUT_MS) return DefaultValues::SESSION_TIMEOUT_MS;
    if (key == ConfigKeys::TASK_TIMEOUT_MS) return DefaultValues::TASK_TIMEOUT_MS;
    if (key == ConfigKeys::CLEANUP_INTERVAL_MS) return DefaultValues::CLEANUP_INTERVAL_MS;
    
    return QVariant();
}

bool ServiceConfig::isValidConfigKey(const QString &key) const
{
    return !key.isEmpty() && key.length() <= 256;
}

void ServiceConfig::notifyConfigChange(const QString &key, const QVariant &value)
{
    emit configChanged(key, value);
}
