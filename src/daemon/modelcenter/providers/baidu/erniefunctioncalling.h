// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ERNIEFUNCTIONCALLING_H
#define ERNIEFUNCTIONCALLING_H

#include "modelcenter/interfaces/functioncallinginterface.h"
#include "erniechat.h"

AIDAEMON_BEGIN_NAMESPACE

namespace ernie {

class ErnieFunctionCalling : public FunctionCallingInterface
{
    Q_OBJECT
public:
    explicit ErnieFunctionCalling(QSharedPointer<ErnieChat> ifs, QObject *parent = nullptr);
    QJsonObject parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params = {}) override;
protected:
    QSharedPointer<ErnieChat> ernieChat() const;
};

}

AIDAEMON_END_NAMESPACE

#endif // ERNIEFUNCTIONCALLING_H
