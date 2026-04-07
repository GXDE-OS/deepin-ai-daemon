// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNIVERSALCHATINTERFACE_H
#define UNIVERSALCHATINTERFACE_H

#include "modelcenter/interfaces/chatinterface.h"

#include <QObject>

AIDAEMON_BEGIN_NAMESPACE

class UniversalChatInterface : public ChatInterface
{
    Q_OBJECT
public:
    explicit UniversalChatInterface(const QString &model, QObject *parent = nullptr);
    ~UniversalChatInterface();
    InterfaceType type() override;
    QJsonObject generate(const QString &content, bool stream, const QList<ChatHistory> &history = {},
                         const QVariantHash &params = {}) override;
    void terminate() override;

    virtual void setApiUrl(const QString &apiUrl);
    virtual void setApiKey(const QString &apiKey);
    virtual QString apiUrl() const;
    virtual QString apiKey() const;

    int lastError() const;
    void setLastError(int error);
    int timeOut() const;
    void setTimeOut(int msec);

signals:
    void eventLoopAbort();
protected slots:
    void onReadDeltaContent(const QByteArray &content);

protected:
    int m_lastError = 0;
    int m_timeOut = 15000;
    QString m_apiUrl;
    QString m_apiKey;

};

AIDAEMON_END_NAMESPACE

#endif // CHATINTERFACE_H
