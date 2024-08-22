// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBUSSERVER_H
#define DBUSSERVER_H

#include "aidaemon_global.h"
#include "apiserverdbus.h"

#include <QThread>

AIDAEMON_BEGIN_NAMESPACE

class DBusServer : public QObject
{
    Q_OBJECT
public:
    explicit DBusServer(QObject *parent = nullptr);
    ~DBusServer();
    void start();
signals:

public slots:
private:
    QScopedPointer<APIServer> apiSrv;
    QThread busWorker;
};

AIDAEMON_END_NAMESPACE

#endif // DBUSSERVER_H
