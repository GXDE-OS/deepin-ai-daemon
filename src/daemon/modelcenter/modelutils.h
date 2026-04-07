// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELUTILS_H
#define MODELUTILS_H

#include "modelcenter_define.h"

#include <QVariant>
#include <QJsonObject>
#include <QVariantHash>
#include <QStringList>

AIDAEMON_BEGIN_NAMESPACE

class ModelUtils
{
public:
    static QList<ChatHistory> jsonToHistory(const QVariantList &list);
    static QPair<int, QString> errorContent(int error);
    static QString providerConfigPath();
    
    // Add filtered parameters from QVariantHash to QJsonObject, excluding specified keys
    static void addFilteredParams(QJsonObject &dataObject, const QVariantHash &params, 
                                  const QStringList &excludeKeys = {"provider", "model", "messages"});
private:
    ModelUtils();
};

AIDAEMON_END_NAMESPACE

#endif // MODELUTILS_H
