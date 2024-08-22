// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelutils.h"

AIDAEMON_USE_NAMESPACE

QList<ChatHistory> ModelUtils::jsonToHistory(const QVariantList &list)
{
    QList<ChatHistory> ret;
    for (const QVariant &line: list) {
        auto map = line.value<QVariantMap>();
        ChatHistory his;
        his.role = map.value("role").toString();
        his.content = map.value("content").toString();
        ret.append(ret);
    }

    return ret;
}

ModelUtils::ModelUtils()
{

}
