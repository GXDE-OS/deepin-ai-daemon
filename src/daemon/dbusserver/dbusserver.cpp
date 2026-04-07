// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbusserver.h"
#include "sessionmanager/sessionmanager.h"
#include "sessionmanagerdbus.h"
#include "modelcenter/modelcenter.h"
#include "taskmanager/taskmanager.h"
#include "modelinfodbus.h"


#include <QDBusConnection>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logDBusServer)

AIDAEMON_USE_NAMESPACE

DBusServer::DBusServer(QObject *parent) : QObject(parent)
{

}

DBusServer::~DBusServer()
{
    dbusWorker.quit();
    dbusWorker.wait(5000);
    
    sessionWorker.quit();
    sessionWorker.wait(5000);
    
    qlWorker.quit();
    qlWorker.wait(5000);
    
    //embWorker.quit();
    //embWorker.wait(5000);

    vecWorker.quit();
    vecWorker.wait(5000);
}

void DBusServer::start()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService("org.deepin.ai.daemon")) {
        qCCritical(logDBusServer) << "Failed to register main service: org.deepin.ai.daemon";
        ::exit(EXIT_FAILURE);
    }

    // Start business logic thread
    sessionWorker.start();
    
    // Create model center in business logic thread
    modelCenter.reset(new ModelCenter);
    modelCenter->moveToThread(&sessionWorker);
    
    // Create session manager with task manager in business logic thread
    sessionManager.reset(new SessionManager);
    sessionManager->moveToThread(&sessionWorker);
    
    // Set up task manager and model center for session manager
    QMetaObject::invokeMethod(sessionManager.data(), [this]() {
        auto taskManager = new TaskManager(sessionManager.data());
        // Enable responsive throttling for cloud services
        taskManager->enableResponsiveThrottling(TaskManager::TaskType::Chat, TaskManager::ModelDeployType::Cloud, true);
        taskManager->enableResponsiveThrottling(TaskManager::TaskType::FunctionCalling, TaskManager::ModelDeployType::Cloud, true);
        sessionManager->setTaskManager(taskManager);
        sessionManager->setModelCenter(modelCenter.data());
    }, Qt::QueuedConnection);
    
    // Start D-Bus interface thread
    dbusWorker.start();
    
    // Create D-Bus interfaces in dedicated D-Bus thread
    sessionManagerDBus.reset(new SessionManagerDBus(sessionManager.data()));
    sessionManagerDBus->moveToThread(&dbusWorker);
    
    modelInfoDBus.reset(new ModelInfoDBus(modelCenter.data()));
    modelInfoDBus->moveToThread(&dbusWorker);
    
    // Register D-Bus services in main thread (required by Qt D-Bus)
    if (!connection.registerService("org.deepin.ai.daemon.SessionManager")) {
        qCCritical(logDBusServer) << "Failed to register SessionManager service: org.deepin.ai.daemon.SessionManager";
        return;
    }
    
    if (!connection.registerObject("/org/deepin/ai/daemon/SessionManager",
                                 sessionManagerDBus.data(), QDBusConnection::ExportAllContents)) {
        qCCritical(logDBusServer) << "Failed to register SessionManager object: /org/deepin/ai/daemon/SessionManager";
        sessionManagerDBus.reset(nullptr);
        return;
    }
    
    qCInfo(logDBusServer) << "SessionManager D-Bus service started successfully";

    if (!connection.registerService("org.deepin.ai.daemon.ModelInfo")) {
        qCCritical(logDBusServer) << "Failed to register ModelInfo service: org.deepin.ai.daemon.ModelInfo";
        return;
    }
    
    if (!connection.registerObject("/org/deepin/ai/daemon/ModelInfo",
                                 modelInfoDBus.data(), QDBusConnection::ExportAllContents)) {
        qCCritical(logDBusServer) << "Failed to register ModelInfo object: /org/deepin/ai/daemon/ModelInfo";
        modelInfoDBus.reset(nullptr);
        return;
    }
    
    qCInfo(logDBusServer) << "ModelInfo D-Bus service started successfully";

    // Start existing query language service
    qlWorker.start();
    qlSrv.reset(new QueryLangDBus);
    qlSrv->start(&qlWorker);

    //embSrv.reset(new EmbPlatformDBus);
    //embSrv->start(&embWorker);
    //embWorker.start();

    //vecSrv.reset(new VectorIndexProxyer(embSrv->handle()));
    //vecSrv->start(&vecWorker);
    //vecWorker.start();

    vecWorker.start();
    vecSrv.reset(new VectorIndexDBus);
    vecSrv->start(&vecWorker);
}
