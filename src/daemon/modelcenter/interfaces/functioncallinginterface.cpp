// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "functioncallinginterface.h"

AIDAEMON_USE_NAMESPACE

FunctionCallingInterface::FunctionCallingInterface(QSharedPointer<ChatInterface> ifs, QObject *parent)
    : ChatInterface(ifs->model(), parent)
    , chatIfs(ifs)
{

}

InterfaceType FunctionCallingInterface::type()
{
    return FunctionCalling;
}

void FunctionCallingInterface::terminate()
{
    chatIfs->terminate();
}

QJsonObject FunctionCallingInterface::generate(const QString &content, bool stream, const QList<ChatHistory> &history, const QVariantHash &params)
{
    return chatIfs->generate(content, stream, history, params);
}

QString FunctionCallingInterface::model() const
{
    return chatIfs->model();
}

void FunctionCallingInterface::setModel(const QString &name)
{
    modelName = name;
    chatIfs->setModel(name);
}
