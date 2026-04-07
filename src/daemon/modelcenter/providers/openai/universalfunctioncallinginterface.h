// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNIVERSALFUNCTIONCALLINGINTERFACE_H
#define UNIVERSALFUNCTIONCALLINGINTERFACE_H

#include "modelcenter/interfaces/functioncallinginterface.h"
#include "universalchatinterface.h"

AIDAEMON_BEGIN_NAMESPACE

class UniversalFunctionCallingInterface : public FunctionCallingInterface
{
    Q_OBJECT
public:
    explicit UniversalFunctionCallingInterface(QSharedPointer<UniversalChatInterface> ifs, QObject *parent = nullptr);
    QJsonObject parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params = {}) override;
protected:
    QSharedPointer<UniversalChatInterface> chatInterface() const;
};

AIDAEMON_END_NAMESPACE

#endif // UNIVERSALFUNCTIONCALLINGINTERFACE_H
