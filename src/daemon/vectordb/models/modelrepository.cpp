// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelrepository.h"
#include "embeddingmodel.h"
#include "modelcenter/modelhub/modelhubwrapper.h"
#include "modelcenter/modelcenter_define.h"
#include "modelcenter/moddelfactory.h"
#include "modelcenter/providers/modelprovider.h"
#include "modelcenter/interfaces/embeddinginterface.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(modelrepo, "vectordb.models")

AIDAEMON_VECTORDB_USE_NAMESPACE

ModelRepository::ModelRepository(QObject *parent)
    : QObject(parent)
{

}

ModelRepository::~ModelRepository()
{

}

QString ModelRepository::defaultModel()
{
    if (m_modelInfo.isEmpty())
        return "";

    //todo get default model from config.
    return m_modelInfo.keys().first();
}

QVariantHash ModelRepository::getModelInfo(const QString &modelId) const
{
    QReadLocker locker(&m_lock);

    if (!m_modelInfo.contains(modelId)) {
        qCWarning(modelrepo) << "ModelRepository: Model ID not found:" << modelId;
        return QVariantHash();
    }

    return m_modelInfo.value(modelId);
}

QStringList ModelRepository::getModelList() const
{
    QReadLocker locker(&m_lock);
    return m_modelInfo.keys();
}

bool ModelRepository::refresh()
{
    {
        QWriteLocker locker(&m_lock);
        m_modelInfo.clear();
    }

    queryInstalledModels();
    loadModelAccountInfo();

    return true;
}

bool ModelRepository::hasModel(const QString &modelId) const
{
    QReadLocker locker(&m_lock);
    return m_modelInfo.contains(modelId);
}

bool ModelRepository::loadModelAccountInfo()
{
    QWriteLocker locker(&m_lock);

    // TODO: Implement model account information loading
    // This is a placeholder for future implementation
    // Could load from configuration files, environment variables, or database
#ifdef QT_DEBUG
    {
        QVariantHash info;
        info[kModelInfoApiKey] = "58ad4ae8-7aaa-4cc0-b909-d3ad10119de4";
        info[kModelInfoApiUrl] = "https://ark.cn-beijing.volces.com/api/v3/embeddings";
        m_modelInfo.insert("openai/doubao-embedding-text-240715", info);
    }
#endif

    qCInfo(modelrepo) << "ModelRepository: Model account info loading placeholder - always returns true";
    return true;
}

bool ModelRepository::queryInstalledModels()
{
    qCDebug(modelrepo) << "ModelRepository: Querying installed models from ModelhubWrapper...";
    auto allModel = ModelhubWrapper::installedModelsInfo();
    QWriteLocker locker(&m_lock);
    for (auto it = allModel.begin(); it != allModel.end(); ++it) {
        auto info = it.value().toHash();
        auto arch = info.value("architectures").toStringList();
        if (arch.contains("embedding", Qt::CaseInsensitive))
            m_modelInfo.insert(QString("modelhub/%0").arg(it.key()), info);
    }

    qCInfo(modelrepo) << "find embedding model in modelhub:" << m_modelInfo.keys();
    return true;
}

QPair<QString, QString> ModelRepository::fromModelId(const QString &modelId) const
{
    int firstSlashIndex = modelId.indexOf('/');
    if (firstSlashIndex > 0 && firstSlashIndex < modelId.length() - 1) {
        QString provider = modelId.left(firstSlashIndex);
        QString model = modelId.mid(firstSlashIndex + 1);
        return qMakePair(provider, model);
    }

    return {};
}

EmbeddingModel *ModelRepository::createModel(const QString &modelId, const QVariantHash &modelInfo)
{
    // Check if model exists
    if (!hasModel(modelId)) {
        qCWarning(modelrepo) << "ModelRepository: Model not found:" << modelId;
        return nullptr;
    }

    auto pm = fromModelId(modelId);

    ModdelFactory factory;
    auto provider = factory.create(pm.first);
    if (provider.isNull()) {
        qCWarning(modelrepo) << "Invaild model id" << modelId << "no such provider" << pm.first;
        return nullptr;
    }

    auto tmp = modelInfo;
    tmp.insert(kModelInfoModel, pm.second);
    AbstractInterface *ifs = provider->createInterface(InterfaceType::TextEmbedding, tmp);
    EmbeddingInterface *emb = dynamic_cast<EmbeddingInterface *>(ifs);
    if (!emb) {
        delete ifs;
        qCWarning(modelrepo) << "fail to create embedding interface for model id" << modelId << "model" << pm.second;
        return nullptr;
    }

   // Create EmbeddingModel instance
    EmbeddingModel *embeddingModel = new EmbeddingModel(modelId, emb, this);
    qCInfo(modelrepo) << "ModelRepository: Successfully created EmbeddingModel for:" << modelId;
    return embeddingModel;
}
