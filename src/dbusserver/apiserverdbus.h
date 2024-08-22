// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef APISERVERDBUS_H
#define APISERVERDBUS_H

#include "aidaemon_global.h"
#include "modelcenter/modelcenter.h"

#include <QObject>
#include <QThread>
#include <QDBusContext>

#include <QMap>

AIDAEMON_BEGIN_NAMESPACE

class APIServerDBus : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.APIServer")
public:
    explicit APIServerDBus(class APIServerWorker *worker, QObject *parent = nullptr);
public slots:
    QString Chat(const QString &content, const QString &params);
private:
    APIServerWorker *apiWorker = nullptr;
};

class APIServerWorker : public QThread
{
    Q_OBJECT
public:
    APIServerWorker(class ModelCenter *model);
public slots:
    void chat(const QString &content, const QString &params, QDBusMessage msg);
private:
    ModelCenter *center = nullptr;
};

class APIServer : public QObject
{
    Q_OBJECT
public:
     explicit APIServer(QObject *parent = nullptr);
     ~APIServer();

     void start(QThread *dbusThread);
private:
    QScopedPointer<APIServerDBus> aDbus;
    QScopedPointer<ModelCenter> center;
    QScopedPointer<APIServerWorker> workerThread;
};

AIDAEMON_END_NAMESPACE

#endif // APISERVERDBUS_H
