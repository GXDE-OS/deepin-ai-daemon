// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FUNCTIONCALLINGINTERFACE_H
#define FUNCTIONCALLINGINTERFACE_H

#include "chatinterface.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>
#include <QSharedPointer>

AIDAEMON_BEGIN_NAMESPACE

class FunctionCallingInterface : public ChatInterface
{
    Q_OBJECT
public:
    explicit FunctionCallingInterface(QSharedPointer<ChatInterface> ifs, QObject *parent = nullptr);
    InterfaceType type() override;
    QString model() const override;
    void setModel(const QString &) override;
    void terminate() override;
    virtual QJsonObject parseFunction(const QString &content, const QJsonArray &func, const QVariantHash &params = {}) = 0;
protected:
    QJsonObject generate(const QString &content, bool stream, const QList<ChatHistory> &history = {},
                                 const QVariantHash &params = {}) override;
protected:
    QSharedPointer<ChatInterface> chatIfs;
};

AIDAEMON_END_NAMESPACE

#endif // FUNCTIONCALLINGINTERFACE_H
