// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelhubfunctioncalling.h"
#include "modelcenter/modelhub/modelhubwrapper.h"

#include <QLoggingCategory>
#include <QJsonArray>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

using namespace modelhub;

ModelhubFunctionCalling::ModelhubFunctionCalling(QSharedPointer<ModelhubChat> ifs, QObject *parent)
    : UniversalFunctionCallingInterface(qSharedPointerCast<UniversalChatInterface>(ifs), parent)
{

}

QJsonObject ModelhubFunctionCalling::parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params)
{
    bool isRun = modelhubChat()->wrapper()->ensureRunning();
    if (!isRun) {
        qCWarning(logModelCenter) << "Failed to ensure model is running, function parsing aborted";
        return QJsonObject();
    }

    qCInfo(logModelCenter) << "Parsing function with content length:" << content.length()
                          << "and" << func.size() << "functions available";
    return UniversalFunctionCallingInterface::parseFunction(content, func, params);
}

QSharedPointer<ModelhubChat> ModelhubFunctionCalling::modelhubChat() const
{
    return qSharedPointerCast<ModelhubChat>(chatIfs);
}
