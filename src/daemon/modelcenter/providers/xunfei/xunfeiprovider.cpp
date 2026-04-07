// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xunfeiprovider_p.h"
#include "xunfeispeechtotext.h"
#include "xunfeitexttospeech.h"
#include "modelcenter/modelutils.h"
#include "config/modelprovidersconfig.h"

#include <QSettings>
#include <QVariant>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

XunfeiProviderPrivate::XunfeiProviderPrivate(XunfeiProvider *parent) : q(parent)
{
}

XunfeiProvider::XunfeiProvider()
    : ModelProvider()
    , d(new XunfeiProviderPrivate(this))
{
}

XunfeiProvider::~XunfeiProvider()
{
    delete d;
    d = nullptr;
}

QString XunfeiProvider::name()
{
    return providerName();
}

AbstractInterface *XunfeiProvider::createInterface(InterfaceType type)
{
    QVariantHash modelInfo;
    ModelProvidersConfig cfg;
    switch (type) {
    case SpeechToText:
        modelInfo = cfg.getProviderConfig(name(), "SpeechToText");
        break;
    case TextToSpeech:
        modelInfo = cfg.getProviderConfig(name(), "TextToSpeech");
        break;
    default:
        qCWarning(logModelCenter) << "Unsupported interface type:" << type << "for Xunfei provider";
        break;
    }

    return createInterface(type, modelInfo);
}

AbstractInterface *XunfeiProvider::createInterface(InterfaceType type, const QVariantHash &modelInfo)
{
    AbstractInterface *ret = nullptr;
    switch (type) {
    case SpeechToText:
        ret = createSpeechToText(modelInfo);
        break;
    case TextToSpeech:
        ret = createTextToSpeech(modelInfo);
        break;
    default:
        qCWarning(logModelCenter) << "Unsupported interface type:" << type << "for Xunfei provider";
        break;
    }

    return ret;
}

SpeechToTextInterface *XunfeiProvider::createSpeechToText(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel, QString("iat")).toString();
    QString appId = modelInfo.value(kModelInfoAppID).toString();
    QString apiKey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiSecret = modelInfo.value(kModelInfoApiSecret).toString();
    QString domain = modelInfo.value("Domain", QString("iat")).toString();
    QString language = modelInfo.value("Language", QString("zh_cn")).toString();
    QString accent = modelInfo.value("Accent", QString("mandarin")).toString();

    if (appId.isEmpty() || apiKey.isEmpty() || apiSecret.isEmpty()) {
        qCWarning(logModelCenter) << "Failed to create speech-to-text interface: missing required credentials";
        return nullptr;
    }

    qCInfo(logModelCenter) << "Creating new XunfeiSpeechToText instance with model:" << model;
    auto speechToText = new XunfeiSpeechToText(model);
    speechToText->setAppId(appId);
    speechToText->setApiKey(apiKey);
    speechToText->setApiSecret(apiSecret);
    speechToText->setDomain(domain);
    speechToText->setLanguage(language);
    speechToText->setAccent(accent);

    return speechToText;
}

TextToSpeechInterface *XunfeiProvider::createTextToSpeech(const QVariantHash &modelInfo)
{
    QString model = modelInfo.value(kModelInfoModel, QString("tts")).toString();
    QString appId = modelInfo.value(kModelInfoAppID).toString();
    QString apiKey = modelInfo.value(kModelInfoApiKey).toString();
    QString apiSecret = modelInfo.value(kModelInfoApiSecret).toString();
    QString voice = modelInfo.value("Voice", QString("x4_yezi")).toString();
    QString audioFormat = modelInfo.value("AudioFormat", QString("raw")).toString();
    QString textEncoding = modelInfo.value("TextEncoding", QString("utf8")).toString();

    if (appId.isEmpty() || apiKey.isEmpty() || apiSecret.isEmpty()) {
        qCWarning(logModelCenter) << "Failed to create text-to-speech interface: missing required credentials";
        return nullptr;
    }

    qCInfo(logModelCenter) << "Creating new XunfeiTextToSpeech instance with model:" << model;
    auto textToSpeech = new XunfeiTextToSpeech(model);
    textToSpeech->setAppId(appId);
    textToSpeech->setApiKey(apiKey);
    textToSpeech->setApiSecret(apiSecret);
    textToSpeech->setVoice(voice);
    textToSpeech->setAudioFormat(audioFormat);
    textToSpeech->setTextEncoding(textEncoding);

    return textToSpeech;
}

XunfeiProvider *XunfeiProvider::create()
{
    return new XunfeiProvider;
} 
