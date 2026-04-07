// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHATINTERFACE_H
#define CHATINTERFACE_H

#include "abstractinterface.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

class ChatInterface : public QObject, public AbstractInterface
{
    Q_OBJECT
public:
    explicit ChatInterface(const QString &model, QObject *parent = nullptr);
    ~ChatInterface() override;
    virtual QString model() const;
    virtual void setModel(const QString &);
    InterfaceType type() override;
    virtual QJsonObject generate(const QString &content, bool stream, const QList<ChatHistory> &history = {},
                                 const QVariantHash &params = {}) = 0;
    virtual void terminate() = 0;
signals:
    void sendDeltaContent(const QString &content);
protected:
    QString modelName;
};

AIDAEMON_END_NAMESPACE

#endif // CHATINTERFACE_H
