// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELCENTER_H
#define MODELCENTER_H

#include "aidaemon_global.h"
#include "modelcenter_define.h"
#include "providers/modelprovider.h"

#include <QObject>

#include <QDBusMessage>

AIDAEMON_BEGIN_NAMESPACE
class ModdelFactory;
class ChatInterface;
class ModelCenter : public QObject
{
    Q_OBJECT
public:
    explicit ModelCenter(QObject *parent = nullptr);
    ~ModelCenter();
signals:
    void requestAbort(const QString &session);
public slots:
    QPair<int, QString> chat(const QString &session, const QString &content, const QList<ChatHistory> &history,
              const QVariantHash &params, ChatStreamer stream, void *user);
    QPair<int, QString> functionCalling(const QString &session, const QString &content, const QJsonArray &functions,
              const QVariantHash &params);
    
    // Helper method to create provider based on params and interface type
    QSharedPointer<ModelProvider> createProviderForInterface(InterfaceType type, const QVariantHash &params, const QString &session);

private:
    ModdelFactory *factory = nullptr;
    
    // Helper method to set model on chat interface if specified in params
    void configureModelForInterface(ChatInterface *interface, const QVariantHash &params, const QString &session);
};

AIDAEMON_END_NAMESPACE

#endif // MODELCENTER_H
