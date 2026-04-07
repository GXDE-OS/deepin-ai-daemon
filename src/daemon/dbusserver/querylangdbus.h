// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef QUERYLANGDBUS_H
#define QUERYLANGDBUS_H

#include "aidaemon_global.h"

#include <QDBusContext>
#include <QThread>
#include <QProcess>
#include <QDBusMessage>

AIDAEMON_BEGIN_NAMESPACE

class QueryLangDBus : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.QueryLang")
public:
    explicit QueryLangDBus(QObject *parent = nullptr);
    void start(QThread *dbusThread);
    QString checkModel();
private:
    QString doQuery(const QByteArray &ba, const QUrl &url);
public slots:
    QString Query(const QString &content);
    bool Enable();
    void SetSemanticOn(bool isTrue);
    QString GetSemanticStatus();

private:

};

AIDAEMON_END_NAMESPACE

#endif // QUERYLANGDBUS_H
