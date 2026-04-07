// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "indexcenter/monitor/filemonitor.h"
#include "dbusserver/dbusserver.h"
#include "config/dconfigmanager.h"
#include "utils/resourcemanager.h"

#include <QGuiApplication>

#include <DLog>

#include <signal.h>
#include <unistd.h>

DCORE_USE_NAMESPACE
AIDAEMON_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logMain)

static void appExitHandler(int sig)
{
    qCInfo(logMain) << "Received signal" << sig << "- initiating application exit";

    // 释放资源
    qApp->quit();
}

static void initLog()
{
    // 设置终端和文件记录日志
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();
}

int main(int argc, char *argv[])
{
    // 安全退出
    signal(SIGINT, appExitHandler);
    signal(SIGQUIT, appExitHandler);
    signal(SIGTERM, appExitHandler);

    QGuiApplication app(argc, argv);
    // 设置应用信息
    app.setOrganizationName("deepin");
    app.setApplicationName("deepin-ai-daemon");

    initLog();
    qCInfo(logMain) << "Application initialized with arguments:" << app.arguments();

    ConfigManagerIns;
    qCInfo(logMain) << "Configuration manager initialized";

    FileMonitor monitor;
    monitor.start(QThread::InheritPriority, 1);
    qCInfo(logMain) << "File monitor started successfully";

    DBusServer server;
    server.start();
    qCInfo(logMain) << "DBus server started successfully";

    ResourceManager::instance()->setAutoReleaseMemory(true);
    qCInfo(logMain) << "Resource manager configured with auto memory release";

    qCInfo(logMain) << "Entering main event loop";
    return app.exec();
}
