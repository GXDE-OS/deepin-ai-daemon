// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOUBAOVISION_H
#define DOUBAOVISION_H

#include "modelcenter/interfaces/imagerecognitioninterface.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

AIDAEMON_BEGIN_NAMESPACE

namespace doubao {

/**
 * @brief DouBao vision recognition implementation
 * 
 * This class implements image recognition using DouBao's vision API.
 * It supports multi-modal input with text prompts and images.
 */
class DouBaoVision : public ImageRecognitionInterface
{
    Q_OBJECT
    
public:
    explicit DouBaoVision(const QString &model, QObject *parent = nullptr);
    ~DouBaoVision() override;
    
    // Configuration setters (called from provider based on config)
    void setApiKey(const QString &apiKey);
    void setApiUrl(const QString &apiUrl);
    void setMaxTokens(int maxTokens);
    void setTemperature(double temperature);
    
    // ImageRecognitionInterface implementation
    QJsonObject recognizeImage(const QList<VisionMessage> &messages, 
                              const QVariantHash &params = {}) override;
    QString startImageRecognition(const QList<VisionMessage> &messages,
                                 const QVariantHash &params = {}) override;
    void terminate() override;
    
    // Information methods
    QStringList getSupportedImageFormats() const override;
    int getMaxImageSize() const override;
    
private slots:
    void onNetworkReplyFinished();
    void onRequestTimeout();
    
private:
    // Helper methods
    QJsonObject buildRequest(const QList<VisionMessage> &messages, const QVariantHash &params);
    QJsonObject parseResponse(const QByteArray &data);
    QString createSessionId();

    // Network and configuration
    QNetworkAccessManager *networkManager;
    QString apiKey;
    QString apiUrl;
    int maxTokens = 4096;
    double temperature = 0.8;
    
    // Request management
    QMap<QNetworkReply*, QString> activeRequests;  // reply -> sessionId
    QTimer *timeoutTimer = nullptr;
    
    // Static configuration
    static const QStringList SUPPORTED_FORMATS;
    static const int MAX_IMAGE_SIZE;
    static const int REQUEST_TIMEOUT;
};

} // namespace doubao

AIDAEMON_END_NAMESPACE

#endif // DOUBAOVISION_H
