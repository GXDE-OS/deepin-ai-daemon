// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SERVICECONFIG_H
#define SERVICECONFIG_H

#include "aidaemon_global.h"

#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QStringList>
#include <QTimer>
#include <QMutex>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Global service configuration manager
 * 
 * Manages global service settings, resource limits, and system-wide
 * parameters that affect all sessions and projects. Implements singleton pattern.
 */
class ServiceConfig : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Get singleton instance
     * @return Pointer to ServiceConfig instance
     */
    static ServiceConfig* instance();

    ~ServiceConfig();

    /**
     * @brief Destroy singleton instance
     */
    static void destroyInstance();

    /**
     * @brief Set configuration value
     * @param key Configuration key
     * @param value Configuration value
     * @return true on success, false on failure
     */
    bool setConfig(const QString &key, const QVariant &value);

    /**
     * @brief Get configuration value
     * @param key Configuration key
     * @param defaultValue Default value if key not found
     * @return Configuration value
     */
    QVariant getConfig(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /**
     * @brief Get all configurations
     * @return All configurations as QVariantMap
     */
    QVariantMap getAllConfigs() const;

    /**
     * @brief Reset all configurations to defaults
     * @return true on success, false on failure
     */
    bool resetToDefaults();

    /**
     * @brief Set resource limit
     * @param resource Resource type
     * @param limit Resource limit value
     * @return true on success, false on failure
     */
    bool setResourceLimit(const QString &resource, const QVariant &limit);

    /**
     * @brief Get resource limit
     * @param resource Resource type
     * @return Resource limit value
     */
    QVariant getResourceLimit(const QString &resource) const;

    /**
     * @brief Check if resource is available
     * @param resource Resource type
     * @param required Required amount
     * @return true if available, false otherwise
     */
    bool isResourceAvailable(const QString &resource, const QVariant &required) const;

    /**
     * @brief Set default model for a type
     * @param modelType Model type (embedding, parser, tts, etc.)
     * @param modelId Model identifier
     * @return true on success, false on failure
     */
    bool setDefaultModel(const QString &modelType, const QString &modelId);

    /**
     * @brief Get default model for a type
     * @param modelType Model type
     * @return Model identifier
     */
    QString getDefaultModel(const QString &modelType) const;

    /**
     * @brief Get available models for a type
     * @param modelType Model type
     * @return List of available model identifiers
     */
    QStringList getAvailableModels(const QString &modelType) const;

    /**
     * @brief Get system status
     * @return System status information
     */
    QVariantMap getSystemStatus() const;

    /**
     * @brief Check if system is healthy
     * @return true if healthy, false otherwise
     */
    bool isSystemHealthy() const;

    /**
     * @brief Save configuration to persistent storage
     * @return true on success, false on failure
     */
    bool saveConfig();

    /**
     * @brief Load configuration from persistent storage
     * @return true on success, false on failure
     */
    bool loadConfig();

    /**
     * @brief Get current resource usage
     * @param resource Resource type
     * @return Current usage amount
     */
    qreal getCurrentResourceUsage(const QString &resource) const;

    /**
     * @brief Get resource usage percentage
     * @param resource Resource type
     * @return Usage percentage (0-100)
     */
    qreal getResourceUsagePercentage(const QString &resource) const;

public Q_SLOTS:
    /**
     * @brief Handle resource usage change
     * @param resource Resource type
     * @param usage Current usage
     */
    void onResourceUsageChanged(const QString &resource, qreal usage);

    /**
     * @brief Handle system status change
     */
    void onSystemStatusChanged();

Q_SIGNALS:
    /**
     * @brief Emitted when configuration changes
     * @param key Configuration key
     * @param value New value
     */
    void configChanged(const QString &key, const QVariant &value);

    /**
     * @brief Emitted when resource limit changes
     * @param resource Resource type
     * @param limit New limit
     */
    void resourceLimitChanged(const QString &resource, const QVariant &limit);

    /**
     * @brief Emitted when system health status changes
     * @param isHealthy Current health status
     */
    void systemHealthChanged(bool isHealthy);

    /**
     * @brief Emitted when resource usage changes
     * @param resource Resource type
     * @param usage Current usage
     * @param percentage Usage percentage
     */
    void resourceUsageChanged(const QString &resource, qreal usage, qreal percentage);

private:
    explicit ServiceConfig(QObject *parent = nullptr);

    static ServiceConfig *s_instance;
    static QMutex s_mutex;

    QSettings *m_settings;
    QVariantMap m_runtimeConfig;
    QVariantMap m_resourceLimits;
    QVariantMap m_resourceUsage;
    QVariantMap m_defaultModels;
    QVariantMap m_availableModels;
    
    mutable QMutex m_configMutex;

    // Resource monitoring
    QTimer *m_resourceMonitorTimer;
    bool m_isSystemHealthy = true;

    // Default configuration values
    void initializeDefaults();
    bool validateConfigValue(const QString &key, const QVariant &value) const;
    bool validateResourceType(const QString &resource) const;
    bool validateModelType(const QString &modelType) const;

    // Resource monitoring
    void updateResourceUsage();
    void checkSystemHealth();
    qreal calculateMemoryUsage() const;
    qreal calculateCpuUsage() const;
    qreal calculateDiskUsage() const;

    // Configuration management
    QString getConfigFilePath() const;
    void loadDefaultConfig();
    void loadUserConfig();
    void saveUserConfig();

    // Helper methods
    QVariant getDefaultValue(const QString &key) const;
    bool isValidConfigKey(const QString &key) const;
    void notifyConfigChange(const QString &key, const QVariant &value);
};

// Configuration key constants
namespace ConfigKeys {
    // Resource limits
    constexpr char MAX_SESSIONS[] = "system/max_sessions";
    constexpr char MAX_MEMORY_MB[] = "system/max_memory_mb";
    constexpr char MAX_CPU_PERCENT[] = "system/max_cpu_percent";
    constexpr char MAX_DISK_GB[] = "system/max_disk_gb";
    constexpr char MAX_CONCURRENT_TASKS[] = "system/max_concurrent_tasks";

    // Default models
    constexpr char DEFAULT_EMBEDDING_MODEL[] = "models/default_embedding";
    constexpr char DEFAULT_PARSER_MODEL[] = "models/default_parser";
    constexpr char DEFAULT_TTS_MODEL[] = "models/default_tts";

    // Performance tuning
    constexpr char THREAD_POOL_SIZE[] = "performance/thread_pool_size";
    constexpr char CACHE_SIZE_MB[] = "performance/cache_size_mb";
    constexpr char INDEX_CACHE_ENABLED[] = "performance/index_cache_enabled";
    constexpr char BATCH_PROCESSING_SIZE[] = "performance/batch_size";

    // Paths and directories
    constexpr char DATA_ROOT_PATH[] = "paths/data_root";
    constexpr char TEMP_PATH[] = "paths/temp";
    constexpr char LOG_PATH[] = "paths/log";
    constexpr char MODEL_PATH[] = "paths/models";

    // Timeouts and intervals
    constexpr char SESSION_TIMEOUT_MS[] = "timeouts/session_timeout";
    constexpr char TASK_TIMEOUT_MS[] = "timeouts/task_timeout";
    constexpr char CLEANUP_INTERVAL_MS[] = "timeouts/cleanup_interval";
}

// Default values
namespace DefaultValues {
    constexpr int MAX_SESSIONS = 50;
    constexpr int MAX_MEMORY_MB = 4096;
    constexpr int MAX_CPU_PERCENT = 80;
    constexpr int MAX_DISK_GB = 100;
    constexpr int MAX_CONCURRENT_TASKS = 10;

    constexpr int THREAD_POOL_SIZE = 8;
    constexpr int CACHE_SIZE_MB = 512;
    constexpr bool INDEX_CACHE_ENABLED = true;
    constexpr int BATCH_PROCESSING_SIZE = 100;

    constexpr int SESSION_TIMEOUT_MS = 1800000; // 30 minutes
    constexpr int TASK_TIMEOUT_MS = 3600000;    // 1 hour
    constexpr int CLEANUP_INTERVAL_MS = 300000; // 5 minutes
}

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // SERVICECONFIG_H
