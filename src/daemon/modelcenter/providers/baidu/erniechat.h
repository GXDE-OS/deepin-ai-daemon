// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ERNIECHAT_H
#define ERNIECHAT_H

#include "modelcenter/interfaces/chatinterface.h"
#include "ernieconversation.h"

#include <QObject>
#include <QMutex>

AIDAEMON_BEGIN_NAMESPACE

namespace ernie {

class ErnieChat : public ChatInterface
{
    Q_OBJECT
public:
    explicit ErnieChat(const QString &model, QObject *parent = nullptr);
    InterfaceType type() override;
    QJsonObject generate(const QString &content, bool stream, const QList<ChatHistory> &history = {},
                         const QVariantHash &params = {}) override;
    void terminate() override;
    static QString modelUrl(const QString &model);
    inline QString getApiKey() const {
        return apiKey;
    }

    inline void setApiKey(const QString &value) {
        if (value.isEmpty())
            return;
        apiKey = value;
    }

    inline QString getApiSecret() const {
            return apiSecret;
    }

    inline void setApiSecret(const QString &value) {
        if (value.isEmpty())
            return;
        apiSecret = value;
    }

    inline QString getApiPath() const {
        return apiPath;
    }

    inline void setApiPath(const QString &value) {
        apiPath = value;
    }

    QString updateAccessToken(const QString &key, const QString &secret);
signals:
    void eventLoopAbort();
protected slots:
    void onReadDeltaContent(const QByteArray &content);
protected:
    QString apiKey;
    QString apiSecret;
    QString apiPath;
    ErnieConversation deltaContent;

    static QString accountID;
    static QString accessToken;
    static QMutex accessMtx;
};

AIDAEMON_END_NAMESPACE
}
#endif // ERNIECHAT_H
