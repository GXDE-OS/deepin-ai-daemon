// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelcenter.h"
#include "modelcenter/moddelfactory.h"
#include "modelcenter/providers/modelprovider.h"
#include "modelcenter/interfaces/chatinterface.h"
#include "modelcenter/interfaces/functioncallinginterface.h"
#include "modelcenter/interfaces/speechtotextinterface.h"
#include "modelutils.h"

#include <QSharedPointer>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

ModelCenter::ModelCenter(QObject *parent)
    : QObject(parent)
    , factory(new ModdelFactory)
{

}

ModelCenter::~ModelCenter()
{
    delete factory;
    factory = nullptr;
}

QSharedPointer<ModelProvider> ModelCenter::createProviderForInterface(InterfaceType type, const QVariantHash &params, const QString &session)
{
    QSharedPointer<ModelProvider> provider;
    
    // Check if specific provider is requested in params
    if (params.contains("provider") && !params.value("provider").toString().isEmpty()) {
        QString providerName = params.value("provider").toString();
        provider = factory->create(providerName);
        qCDebug(logModelCenter) << "Using specified provider for session" << session << ":" << providerName;
    } else {
        provider = factory->createDefault(type);
        qCDebug(logModelCenter) << "Using default provider for session" << session;
    }
    
    if (provider.isNull()) {
        qCWarning(logModelCenter) << "Failed to create provider for session:" << session;
        return nullptr;
    }
    
    return provider;
}

void ModelCenter::configureModelForInterface(ChatInterface *interface, const QVariantHash &params, const QString &session)
{
    // Check if specific model is requested in params
    if (params.contains("model") && !params.value("model").toString().isEmpty()) {
        QString modelName = params.value("model").toString();
        interface->setModel(modelName);
        qCDebug(logModelCenter) << "Set custom model for session" << session << ":" << modelName;
    } else {
        qCDebug(logModelCenter) << "Using default model for session" << session;
    }
}

QPair<int, QString> ModelCenter::chat(const QString &session, const QString &content, const QList<ChatHistory> &history,
                       const QVariantHash &params, ChatStreamer stream, void *user)
{
    qCInfo(logModelCenter) << "Starting chat session:" << session << "with content length:" << content.length();
    
    QSharedPointer<ModelProvider> provider = createProviderForInterface(Chat, params, session);
    if (provider.isNull()) {
        return ModelUtils::errorContent(1);
    }

    QSharedPointer<ChatInterface> chatIfs(dynamic_cast<ChatInterface *>(provider->createInterface(Chat)));
    if (chatIfs.isNull()) {
        qCWarning(logModelCenter) << "Failed to create chat interface for session:" << session;
        return ModelUtils::errorContent(1);
    }

    configureModelForInterface(chatIfs.data(), params, session);

    ChatInterface *chatPtr = chatIfs.data();
    if (stream) {
        connect(chatIfs.data(), &ChatInterface::sendDeltaContent, chatIfs.data(), [chatPtr, stream, user](const QString &delata){
           if (!stream(delata, user))
               chatPtr->terminate();
        });
    }

    connect(this, &ModelCenter::requestAbort, chatIfs.data(), [chatPtr, session](const QString &sess){
        if (session == sess)
            chatPtr->terminate();
    });

    QJsonObject response = chatIfs->generate(content, true, history, params);
    qCDebug(logModelCenter) << "Chat session" << session << "completed with response length:" 
                           << response["content"].toString().length();

    return {0, response["content"].toString()};
}

QPair<int, QString> ModelCenter::functionCalling(const QString &session, const QString &content,
                                                 const QJsonArray &functions, const QVariantHash &params)
{
    qCInfo(logModelCenter) << "Starting function calling session:" << session 
                          << "with" << functions.size() << "functions";

    QSharedPointer<ModelProvider> provider = createProviderForInterface(FunctionCalling, params, session);
    if (provider.isNull()) {
        return ModelUtils::errorContent(1);
    }

    QSharedPointer<FunctionCallingInterface> funcIfs(dynamic_cast<FunctionCallingInterface *>(provider->createInterface(FunctionCalling)));
    if (funcIfs.isNull()) {
        qCWarning(logModelCenter) << "Failed to create function calling interface for session:" << session;
        return ModelUtils::errorContent(1);
    }

    configureModelForInterface(funcIfs.data(), params, session);

    FunctionCallingInterface *funcPtr = funcIfs.data();
    connect(this, &ModelCenter::requestAbort, funcIfs.data(), [funcPtr, session](const QString &sess){
        if (session == sess)
            funcPtr->terminate();
    });

    QJsonObject response = funcPtr->parseFunction(content, functions, params);
    if (response.contains("function")) {
        qCDebug(logModelCenter) << "Function calling session" << session << "completed with function response";
        return {0,  response.value("function").toString()};
    }
    
    qCDebug(logModelCenter) << "Function calling session" << session << "completed with content response";
    return {20, response["content"].toString()};
}
