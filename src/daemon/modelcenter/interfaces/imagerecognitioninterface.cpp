// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagerecognitioninterface.h"

AIDAEMON_USE_NAMESPACE

ImageRecognitionInterface::ImageRecognitionInterface(const QString &model, QObject *parent)
    : QObject(parent), modelName(model)
{
}

ImageRecognitionInterface::~ImageRecognitionInterface()
{
}

QString ImageRecognitionInterface::model() const
{
    return modelName;
}

void ImageRecognitionInterface::setModel(const QString &model)
{
    modelName = model;
}

InterfaceType ImageRecognitionInterface::type()
{
    return ImageRecognition;
}

