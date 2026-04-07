// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "chatinterface.h"

AIDAEMON_USE_NAMESPACE


ChatInterface::ChatInterface(const QString &model, QObject *parent)
    : QObject(parent)
    , AbstractInterface()
    , modelName(model)
{

}

ChatInterface::~ChatInterface()
{

}

QString ChatInterface::model() const
{
    return modelName;
}

void ChatInterface::setModel(const QString &name)
{
    modelName = name;
}

InterfaceType ChatInterface::type()
{
    return Chat;
}
