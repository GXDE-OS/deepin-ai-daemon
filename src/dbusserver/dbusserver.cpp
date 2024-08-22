// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dbusserver.h"


AIDAEMON_USE_NAMESPACE

DBusServer::DBusServer(QObject *parent) : QObject(parent)
{

}

DBusServer::~DBusServer()
{
    busWorker.terminate();
    busWorker.wait(5000);
}

void DBusServer::start()
{
    busWorker.start();

    apiSrv.reset(new APIServer);
    apiSrv->start(&busWorker);
}
