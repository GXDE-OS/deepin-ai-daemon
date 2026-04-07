// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocrinterface.h"

AIDAEMON_USE_NAMESPACE

OCRInterface::OCRInterface(const QString &model, QObject *parent)
    : QObject(parent), modelName(model)
{
}

OCRInterface::~OCRInterface()
{
}

QString OCRInterface::model() const
{
    return modelName;
}

void OCRInterface::setModel(const QString &model)
{
    modelName = model;
}

InterfaceType OCRInterface::type()
{
    return OCR;
}
