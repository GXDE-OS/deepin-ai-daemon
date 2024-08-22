// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELUTILS_H
#define MODELUTILS_H

#include "modelcenter_define.h"

#include <QVariant>

AIDAEMON_BEGIN_NAMESPACE

class ModelUtils
{
public:
    static QList<ChatHistory> jsonToHistory(const QVariantList &list);
private:
    ModelUtils();
};

AIDAEMON_END_NAMESPACE

#endif // MODELUTILS_H
