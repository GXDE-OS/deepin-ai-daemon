// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELHUBCHAT_H
#define MODELHUBCHAT_H

#include "modelcenter/providers/openai/universalchatinterface.h"

#include <QSharedPointer>

class ModelhubWrapper;

AIDAEMON_BEGIN_NAMESPACE

namespace modelhub {

class ModelhubChat : public UniversalChatInterface
{
public:
    explicit ModelhubChat(const QString &model, QSharedPointer<ModelhubWrapper> wrapper, QObject *parent = nullptr);
    ~ModelhubChat();
    QJsonObject generate(const QString &content, bool stream, const QList<ChatHistory> &history = {},
                                 const QVariantHash &params = {}) override;
    QString apiUrl() const override;
    inline QSharedPointer<ModelhubWrapper> wrapper() const {
        return m_wrapper;
    }
protected:
    QSharedPointer<ModelhubWrapper> m_wrapper;
};

}

AIDAEMON_END_NAMESPACE

#endif // MODELHUBCHAT_H
