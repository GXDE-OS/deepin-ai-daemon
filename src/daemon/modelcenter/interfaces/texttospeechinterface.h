// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXTTOSPEECHINTERFACE_H
#define TEXTTOSPEECHINTERFACE_H

#include "abstractinterface.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantHash>

AIDAEMON_BEGIN_NAMESPACE

class TextToSpeechInterface : public QObject, public AbstractInterface
{
    Q_OBJECT
public:
    explicit TextToSpeechInterface(const QString &model, QObject *parent = nullptr);
    ~TextToSpeechInterface();
    
    virtual QString model() const;
    virtual void setModel(const QString &model);
    InterfaceType type() override;
    
    // Stream-based synthesis
    virtual QString startStreamSynthesis(const QString &text,
                                        const QVariantHash &params = {}) = 0;
    virtual QJsonObject endStreamSynthesis(const QString &sessionId) = 0;
    
    // Information methods
    virtual QStringList getSupportedVoices() const = 0;
    
    // Control methods
    virtual void terminate() = 0;
    
signals:
    // Stream synthesis results
    void synthesisResult(const QString &sessionId, const QByteArray &audioData);
    void synthesisError(const QString &sessionId, int errorCode, const QString &message);
    void synthesisCompleted(const QString &sessionId, const QByteArray &finalAudio);
    
protected:
    QString modelName;
};

AIDAEMON_END_NAMESPACE

#endif // TEXTTOSPEECHINTERFACE_H
