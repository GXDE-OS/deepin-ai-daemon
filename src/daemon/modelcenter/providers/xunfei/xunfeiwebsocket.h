// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEIWEBSOCKET_H
#define XUNFEIWEBSOCKET_H

#include "aidaemon_global.h"

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>

AIDAEMON_BEGIN_NAMESPACE

class XunfeiWebSocket : public QObject
{
    Q_OBJECT
public:
    explicit XunfeiWebSocket(QObject *parent = nullptr);
    ~XunfeiWebSocket();
    
    // Configuration
    void setAppId(const QString &appId);
    void setApiKey(const QString &apiKey);
    void setApiSecret(const QString &apiSecret);
    void setDomain(const QString &domain);
    void setLanguage(const QString &language);
    void setAccent(const QString &accent);
    
    // Connection management
    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const;
    bool isConnecting() const;
    
    // Audio data transmission
    void sendFirstFrame(const QByteArray &audioData);
    void sendContinueFrame(const QByteArray &audioData);
    void sendLastFrame(const QByteArray &audioData);
    
    // Session management
    QString sessionId() const;
    
signals:
    void connected();
    void disconnected();
    void recognitionResult(const QString &text);
    void recognitionPartialResult(const QString &partialText);
    void recognitionCompleted(const QString &finalText);
    void error(int errorCode, const QString &errorMessage);
    
private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError> &errors);
    
private:
    QString generateAuthUrl();
    QJsonObject createAudioMessage(const QByteArray &audioData, int status);
    void processRecognitionMessage(const QJsonObject &message);
    
private:
    QWebSocket *webSocket = nullptr;
    QString appId;
    QString apiKey;
    QString apiSecret;
    QString domain;
    QString language;
    QString accent;
    QString currentSessionId;
    QString currentRecognitionResult;
    
    static constexpr int STATUS_FIRST_FRAME = 0;
    static constexpr int STATUS_CONTINUE_FRAME = 1;
    static constexpr int STATUS_LAST_FRAME = 2;
};

AIDAEMON_END_NAMESPACE

#endif // XUNFEIWEBSOCKET_H 
