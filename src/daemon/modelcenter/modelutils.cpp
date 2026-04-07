// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelutils.h"

#include <QGuiApplication>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>

AIDAEMON_USE_NAMESPACE

QList<ChatHistory> ModelUtils::jsonToHistory(const QVariantList &list)
{
    QList<ChatHistory> ret;
    for (const QVariant &line: list) {
        auto map = line.value<QVariantMap>();
        ChatHistory his;
        his.role = map.value("role").toString();
        his.content = map.value("content").toString();
        ret.append(his);
    }

    return ret;
}

QPair<int, QString> ModelUtils::errorContent(int error)
{
    QPair<int, QString> ret;
    switch (error) {
    case 1:
        ret = {1, QString("Model is invalid.")};
        break;
    default:
        break;
    }
    return ret;
}

QString ModelUtils::providerConfigPath()
{
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    configPath = configPath
            + "/" + qApp->organizationName()
            + "/" + qApp->applicationName()
            + "/" + "modelproviders" + ".json";

    return configPath;
}

void ModelUtils::addFilteredParams(QJsonObject &dataObject, const QVariantHash &params, const QStringList &excludeKeys)
{
    // Add all parameters from params except those in excludeKeys
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QString &key = it.key();
        // Skip keys that are in the exclude list
        if (!excludeKeys.contains(key)) {
            dataObject.insert(key, QJsonValue::fromVariant(it.value()));
        }
    }
}

ModelUtils::ModelUtils()
{

}
