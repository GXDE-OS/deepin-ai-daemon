// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNIVERSALEMBEDDINGINTERFACE_H
#define UNIVERSALEMBEDDINGINTERFACE_H

#include "modelcenter/interfaces/embeddinginterface.h"

#include <QObject>

AIDAEMON_BEGIN_NAMESPACE

class UniversalEmbeddingInterface : public EmbeddingInterface
{
    Q_OBJECT
public:
    explicit UniversalEmbeddingInterface(const QString &model, QObject *parent = nullptr);
    ~UniversalEmbeddingInterface();

    QJsonObject embeddings(const QStringList &texts, const QVariantHash &params = {}) override;
    void terminate() override;

    // API configuration methods
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

protected:
    QJsonObject processEmbeddingResponse(const QByteArray &response);
protected:
    // OpenAI embedding model configuration
    int m_lastError = 0;
    int m_timeOut = 15000;
    QString m_apiUrl;
    QString m_apiKey;

};

AIDAEMON_END_NAMESPACE

#endif // UNIVERSALEMBEDDINGINTERFACE_H 
