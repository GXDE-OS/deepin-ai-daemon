// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/deployerwidget.h"
#include "logcategory.h"

#include <DApplication>
#include <DMainWindow>
#include <DLabel>
#include <DLog>

#include <signal.h>

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE
AIDAEMON_USE_NAMESPACE

static void appExitHandler(int sig)
{
    qCInfo(logDeployerMain) << "Received signal" << sig << "- initiating application exit";
    qApp->quit();
}

static void initLog()
{
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();
}

int main(int argc, char *argv[])
{
    // Exit Safely
    signal(SIGINT, appExitHandler);
    signal(SIGQUIT, appExitHandler);
    signal(SIGTERM, appExitHandler);

    DApplication a(argc, argv);
    a.setOrganizationName("deepin");
    a.setApplicationName("deepin-ai-deployer");

    initLog();
    qCInfo(logDeployerMain) << "Application initialized with arguments:" << a.arguments();

    DeployerWidget w;
    qCInfo(logDeployerMain) << "DeployerWidget created successfully";
    
    w.show();
    qCInfo(logDeployerMain) << "Entering main event loop";
    
    return a.exec();
}
