// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelhubembedding.h"
#include "modelcenter/modelhub/modelhubwrapper.h"
#include <QLoggingCategory>

AIDAEMON_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

using namespace modelhub;

ModelhubEmbedding::ModelhubEmbedding(const QString &model, QSharedPointer<ModelhubWrapper> wrapper, QObject *parent)
    : UniversalEmbeddingInterface(model, parent)
    , m_wrapper(wrapper)
{

}

ModelhubEmbedding::~ModelhubEmbedding()
{

}

QJsonObject ModelhubEmbedding::embeddings(const QStringList &texts, const QVariantHash &params)
{
    bool isRun = m_wrapper->ensureRunning();
    if (!isRun) {
        qCWarning(logModelCenter) << "Modelhub service is not running, cannot generate embeddings";
        QJsonObject errorResponse;
        errorResponse["error"] = "Modelhub service is not running";
        errorResponse["code"] = 503;
        return errorResponse;
    }

    qCDebug(logModelCenter) << "Generating embeddings for" << texts.size() << "texts with model:" << model();
    return UniversalEmbeddingInterface::embeddings(texts, params);
}

QString ModelhubEmbedding::apiUrl() const
{
    return m_wrapper->urlPath("/embeddings");
}
