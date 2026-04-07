// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELHUBFUNCTIONCALLING_H
#define MODELHUBFUNCTIONCALLING_H

#include "modelcenter/providers/openai/universalfunctioncallinginterface.h"
#include "modelhubchat.h"

AIDAEMON_BEGIN_NAMESPACE

namespace modelhub {

class ModelhubFunctionCalling : public UniversalFunctionCallingInterface
{
    Q_OBJECT
public:
    explicit ModelhubFunctionCalling(QSharedPointer<ModelhubChat> ifs, QObject *parent = nullptr);
    QJsonObject parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params = {}) override;
protected:
    QSharedPointer<ModelhubChat> modelhubChat() const;
};

}

AIDAEMON_END_NAMESPACE

#endif // MODELHUBFUNCTIONCALLING_H
