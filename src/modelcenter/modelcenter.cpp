// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelcenter.h"

#include <QTimer>
#include <QEventLoop>

AIDAEMON_USE_NAMESPACE

ModelCenter::ModelCenter(QObject *parent) : QObject(parent)
{

}

QPair<int, QString> ModelCenter::chat(const QString &content, const QList<ChatHistory> &history,
                       const QVariantHash &params, ChatStreamer stream, void *user)
{
    return {0, ""};
}
