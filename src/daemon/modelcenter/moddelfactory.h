// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODDELFACTORY_H
#define MODDELFACTORY_H

#include "modelcenter_define.h"

#include <QObject>
#include <QHash>

#include <functional>

AIDAEMON_BEGIN_NAMESPACE

class ModelProvider;
using CreateModel = std::function<ModelProvider *()>;

class ModdelFactory : public QObject
{
    Q_OBJECT
public:
    explicit ModdelFactory(QObject *parent = nullptr);
    QSharedPointer<ModelProvider> create(const QString &name);
    QSharedPointer<ModelProvider> createDefault(InterfaceType type);
signals:

public slots:

private:
    QHash<QString, CreateModel> providers;
};

AIDAEMON_END_NAMESPACE

#endif // MODDELFACTORY_H