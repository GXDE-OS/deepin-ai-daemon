// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SPEECHTOTEXTINTERFACE_H
#define SPEECHTOTEXTINTERFACE_H

#include "abstractinterface.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

class SpeechToTextInterface : public QObject, public AbstractInterface
{
    Q_OBJECT
public:
    explicit SpeechToTextInterface(const QString &model, QObject *parent = nullptr);
    ~SpeechToTextInterface();
    
    virtual QString model() const;
    virtual void setModel(const QString &model);
    InterfaceType type() override;
    
    // File-based recognition
    virtual QJsonObject recognizeFile(const QString &audioFile, 
                                     const QVariantHash &params = {}) = 0;
    
    // Stream-based recognition
    virtual QString startStreamRecognition(const QVariantHash &params = {}) = 0;
    virtual bool sendAudioData(const QString &sessionId, 
                              const QByteArray &audioData) = 0;
    virtual QJsonObject endStreamRecognition(const QString &sessionId) = 0;
    
    // Information methods
    virtual QStringList getSupportedFormats() const = 0;
    
    // Control methods
    virtual void terminate() = 0;
    
signals:
    // Stream recognition results
    void recognitionResult(const QString &sessionId, const QString &text);
    void recognitionPartialResult(const QString &sessionId, const QString &partialText);
    void recognitionError(const QString &sessionId, int errorCode, const QString &message);
    void recognitionCompleted(const QString &sessionId, const QString &finalText);
    
protected:
    QString modelName;
};

AIDAEMON_END_NAMESPACE

#endif // SPEECHTOTEXTINTERFACE_H 