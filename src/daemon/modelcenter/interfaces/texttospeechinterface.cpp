// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "texttospeechinterface.h"

AIDAEMON_USE_NAMESPACE

TextToSpeechInterface::TextToSpeechInterface(const QString &model, QObject *parent)
    : QObject(parent), modelName(model)
{
}

TextToSpeechInterface::~TextToSpeechInterface()
{
}

QString TextToSpeechInterface::model() const
{
    return modelName;
}

void TextToSpeechInterface::setModel(const QString &model)
{
    modelName = model;
}

InterfaceType TextToSpeechInterface::type()
{
    return TextToSpeech;
}
