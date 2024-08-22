// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "apiserverdbus.h"
#include "modelcenter/modelutils.h"

#include <QDBusConnection>
#include <QDebug>
#include <QApplication>
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>

AIDAEMON_USE_NAMESPACE

APIServerDBus::APIServerDBus(APIServerWorker *worker, QObject *parent)
    : QObject(parent)
    , QDBusContext()
    , apiWorker(worker)
{

}

QString APIServerDBus::Chat(const QString &content, const QString &params)
{
    if (content.isEmpty())
        return "";

    QDBusMessage msg = message();
    msg.setDelayedReply(true);

    QMetaObject::invokeMethod(apiWorker, "chat", Q_ARG(QString, content), Q_ARG(QString, params), Q_ARG(QDBusMessage, msg));

    return "";
}

APIServerWorker::APIServerWorker(ModelCenter *model)
    : QThread()
    , center(model)
{

}

void APIServerWorker::chat(const QString &content, const QString &params, QDBusMessage msg)
{
    auto con = QDBusConnection::sessionBus();
    QJsonDocument doc = QJsonDocument::fromJson(params.toUtf8());
    QVariantHash varParams = doc.object().toVariantHash();
    QSharedPointer<QLocalSocket> streamSocket;
    ChatStreamer streamer = nullptr;
    if (varParams.contains(kChatParamsStream)) {
        QString path = varParams.value(kChatParamsStream).toString();
        if (path.isEmpty()) {
            // todo error code;
            con.send(msg.createReply(""));
            return;
        }
        streamSocket.reset(new QLocalSocket);
        streamSocket->connectToServer(path);
        streamer = [streamSocket](const QString &text, void *user) {
            if (streamSocket->state() != QLocalSocket::ConnectedState)
                return false;
            if (streamSocket->write(text.toUtf8()) == -1)
                return false;

            return true;
        };
    }

    auto history = ModelUtils::jsonToHistory(varParams.value(kChatParamsMessages).toList());
    auto result = center->chat(content, history, varParams, streamer, nullptr);

    QVariantHash out;
    if (result.first == 0) {
        out.insert("content", result.second);
    } else {
        out.insert("error", result.first);
        out.insert("errorMessage", result.second);
    }

    {
        QJsonObject obj = QJsonObject::fromVariantHash(out);
        doc = QJsonDocument(obj);
        con.send(msg.createReply(doc.toJson()));
    }
}

APIServer::APIServer(QObject *parent)
    : QObject(parent)
{

}

APIServer::~APIServer()
{

}

void APIServer::start(QThread *dbusThread)
{
    if (!aDbus.isNull())
        return;

    center.reset(new ModelCenter);
    workerThread.reset(new APIServerWorker(center.data()));
    center->moveToThread(workerThread.data());

    qInfo() << "Init DBus APIServer start";
    auto session = QDBusConnection::sessionBus();
    if (!session.registerService("org.deepin.ai.daemon.APIServer")) {
        qWarning("Cannot register the \"org.deepin.ai.daemon.APIServer\" service.\n");
        return;
    }

    aDbus.reset(new APIServerDBus(workerThread.data()));
    if (!session.registerObject("/org/deepin/ai/daemon/APIServer",
                             aDbus.data(), QDBusConnection::ExportAllContents)) {
        qWarning("Cannot register the \"/org/deepin/ai/daemon/APIServer\" object.\n");
        aDbus.reset(nullptr);
        return;
    }

    if (dbusThread)
        aDbus->moveToThread(dbusThread);

    workerThread->start();
    qInfo() << "Init DBus APIServer end";
}


