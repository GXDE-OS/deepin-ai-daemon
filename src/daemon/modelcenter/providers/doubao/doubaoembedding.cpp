// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "doubaoembedding.h"

AIDAEMON_USE_NAMESPACE

using namespace doubao;

DouBaoEmbedding::DouBaoEmbedding(const QString &model, QObject *parent)
    : UniversalEmbeddingInterface(model, parent)
{
    m_apiUrl = "https://ark.cn-beijing.volces.com/api/v3/embeddings";
}
