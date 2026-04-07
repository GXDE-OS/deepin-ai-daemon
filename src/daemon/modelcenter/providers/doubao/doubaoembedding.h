// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOUBAOEMBEDDING_H
#define DOUBAOEMBEDDING_H

#include "modelcenter/providers/openai/universalembeddinginterface.h"

AIDAEMON_BEGIN_NAMESPACE

namespace doubao {

class DouBaoEmbedding : public UniversalEmbeddingInterface
{
public:
    explicit DouBaoEmbedding(const QString &model, QObject *parent = nullptr);
};

}
AIDAEMON_END_NAMESPACE
#endif // DOUBAOEMBEDDING_H
