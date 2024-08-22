// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELCENTER_H
#define MODELCENTER_H

#include "aidaemon_global.h"
#include "modelcenter_define.h"

#include <QObject>

#include <QDBusMessage>

AIDAEMON_BEGIN_NAMESPACE

class ModelCenter : public QObject
{
    Q_OBJECT
public:
    explicit ModelCenter(QObject *parent = nullptr);

signals:

public slots:
    QPair<int, QString> chat(const QString &content, const QList<ChatHistory> &history,
              const QVariantHash &params, ChatStreamer stream, void *user);
};

AIDAEMON_END_NAMESPACE

#endif // MODELCENTER_H
