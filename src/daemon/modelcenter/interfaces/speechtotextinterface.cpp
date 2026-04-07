// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "speechtotextinterface.h"

AIDAEMON_USE_NAMESPACE

SpeechToTextInterface::SpeechToTextInterface(const QString &model, QObject *parent)
    : QObject(parent), modelName(model)
{
}

SpeechToTextInterface::~SpeechToTextInterface()
{
}

QString SpeechToTextInterface::model() const
{
    return modelName;
}

void SpeechToTextInterface::setModel(const QString &model)
{
    modelName = model;
}

InterfaceType SpeechToTextInterface::type()
{
    return SpeechToText;
} 