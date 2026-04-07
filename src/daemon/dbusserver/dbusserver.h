// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBUSSERVER_H
#define DBUSSERVER_H

#include "aidaemon_global.h"
#include "sessionmanagerdbus.h"
#include "querylangdbus.h"
//#include "embplatformdbus.h"
//#include "vectorindexproxyer.h"
#include "vectorindexdbus.h"

#include <QThread>

AIDAEMON_BEGIN_NAMESPACE

class SessionManager;
class ModelCenter;
class ModelInfoDBus;

class DBusServer : public QObject
{
    Q_OBJECT
public:
    explicit DBusServer(QObject *parent = nullptr);
    ~DBusServer();
    void start();

private:
    // New architecture components
    QScopedPointer<SessionManager> sessionManager;
    QScopedPointer<SessionManagerDBus> sessionManagerDBus;
    QScopedPointer<ModelCenter> modelCenter;
    QScopedPointer<ModelInfoDBus> modelInfoDBus;

    QThread sessionWorker;  // Business logic thread
    QThread dbusWorker;     // D-Bus interface thread
    
    // Existing query language service
    QScopedPointer<QueryLangDBus> qlSrv;
    QThread qlWorker;

    // embedding
    //QScopedPointer<EmbPlatformDBus> embSrv;
    //QThread embWorker;

    // old vector index
    //QScopedPointer<VectorIndexProxyer> vecSrv;
    QScopedPointer<VectorIndexDBus> vecSrv;
    QThread vecWorker;
};

AIDAEMON_END_NAMESPACE

#endif // DBUSSERVER_H
