// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEITTSWEBSOCKET_H
#define XUNFEITTSWEBSOCKET_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QString>
#include <QLoggingCategory>
#include <QUuid>
#include "modelcenter/modelcenter_define.h"

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_BEGIN_NAMESPACE

class XunfeiTTSWebSocket : public QObject
{
    Q_OBJECT
public:
    explicit XunfeiTTSWebSocket(QObject *parent = nullptr);
    ~XunfeiTTSWebSocket();
    
    // Configuration methods
    void setAppId(const QString &appId);
    void setApiKey(const QString &apiKey);
    void setApiSecret(const QString &apiSecret);
    void setVoice(const QString &voice);
    void setAudioFormat(const QString &audioFormat);
    void setTextEncoding(const QString &textEncoding);
    
    // Connection management
    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;
    bool isConnecting() const;
    
    // Text synthesis
    void synthesizeText(const QString &text);
    
    // Session management
    QString sessionId() const;
    
signals:
    void connected();
    void disconnected();
    void synthesisResult(const QByteArray &audioData);
    void synthesisCompleted(const QByteArray &finalAudio);
    void error(int errorCode, const QString &errorMessage);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);
    
private:
    QString generateAuthUrl();
    QJsonObject createSynthesisMessage(const QString &text);
    void processSynthesisMessage(const QJsonObject &message);
    
private:
    QWebSocket *webSocket = nullptr;
    QString appId;
    QString apiKey;
    QString apiSecret;
    QString voice;
    QString audioFormat;
    QString textEncoding;
    QString currentSessionId;
    QByteArray currentAudioData;
    
    static constexpr int STATUS_FIRST_FRAME = 0;
    static constexpr int STATUS_CONTINUE_FRAME = 1;
    static constexpr int STATUS_LAST_FRAME = 2;
};

AIDAEMON_END_NAMESPACE

#endif // XUNFEITTSWEBSOCKET_H
