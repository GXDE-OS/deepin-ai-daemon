// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELHUBEMBEDDING_H
#define MODELHUBEMBEDDING_H

#include "modelcenter/providers/openai/universalembeddinginterface.h"

#include <QSharedPointer>

class ModelhubWrapper;

AIDAEMON_BEGIN_NAMESPACE

namespace modelhub {

class ModelhubEmbedding : public UniversalEmbeddingInterface
{
public:
    explicit ModelhubEmbedding(const QString &model, QSharedPointer<ModelhubWrapper> wrapper, QObject *parent = nullptr);
    ~ModelhubEmbedding();
    QJsonObject embeddings(const QStringList &texts, const QVariantHash &params = {}) override;
    QString apiUrl() const override;
    inline QSharedPointer<ModelhubWrapper> wrapper() const {
        return m_wrapper;
    }
protected:
    QSharedPointer<ModelhubWrapper> m_wrapper;
};

}

AIDAEMON_END_NAMESPACE

#endif // MODELHUBEMBEDDING_H
