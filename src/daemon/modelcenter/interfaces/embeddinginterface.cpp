// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "embeddinginterface.h"
#include "modelcenter/modelcenter_define.h"

AIDAEMON_USE_NAMESPACE

EmbeddingInterface::EmbeddingInterface(const QString &model, QObject *parent)
    : QObject(parent)
    , modelName(model)
{

}

EmbeddingInterface::~EmbeddingInterface()
{

}

QString EmbeddingInterface::model() const
{
    return modelName;
}

void EmbeddingInterface::setModel(const QString &model)
{
    modelName = model;
}

int EmbeddingInterface::batchSize() const
{
    //default
    return 5;
}

InterfaceType EmbeddingInterface::type()
{
    return TextEmbedding;
}
