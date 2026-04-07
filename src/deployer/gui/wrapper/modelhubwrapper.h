// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MODELHUBWRAPPER_H
#define MODELHUBWRAPPER_H
#include "aidaemon_global.h"

#include <QObject>
#include <QReadWriteLock>
AIDAEMON_BEGIN_NAMESPACE
class ModelhubWrapper : public QObject
{
    Q_OBJECT
public:
    explicit ModelhubWrapper(const QString &model, QObject *parent = nullptr);
    ~ModelhubWrapper();
    bool isRunning();
    bool ensureRunning();
    bool health();
    bool stopModel();
    void setKeepLive(bool live);
    QString urlPath(const QString &api) const;
    static bool isModelhubInstalled();
    static bool isModelInstalled(const QString &model);
    static QVariantHash modelStatus(const QString &model);
    static QList<QVariantHash> modelsStatus();
    static bool openCmd(const QString &cmd, QString &out);
    static QStringList installedModels();

protected:
    void updateHost();
protected:
    bool keepLive = false;
    QString modelName;
    QString host;
    int port = -1;
    bool started = false;
    qint64 pid = -1;
    mutable QReadWriteLock lock;
};
AIDAEMON_END_NAMESPACE
#endif   // MODELHUBWRAPPER_H
