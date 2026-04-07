// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEIPROVIDER_H
#define XUNFEIPROVIDER_H

#include "modelcenter/providers/modelprovider.h"

AIDAEMON_BEGIN_NAMESPACE

class SpeechToTextInterface;
class TextToSpeechInterface;
class XunfeiProviderPrivate;

class XunfeiProvider : public ModelProvider
{
    friend class XunfeiProviderPrivate;
public:
    static inline QString providerName() {
        return "xunfei";
    }
    static XunfeiProvider *create();
    
    XunfeiProvider();
    ~XunfeiProvider();
    
public:
    QString name() override;
    AbstractInterface *createInterface(InterfaceType type) override;
    AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) override;
    
protected:
    SpeechToTextInterface *createSpeechToText(const QVariantHash &modelInfo);
    TextToSpeechInterface *createTextToSpeech(const QVariantHash &modelInfo);    
private:
    XunfeiProviderPrivate *d = nullptr;
};

AIDAEMON_END_NAMESPACE

#endif // XUNFEIPROVIDER_H 
