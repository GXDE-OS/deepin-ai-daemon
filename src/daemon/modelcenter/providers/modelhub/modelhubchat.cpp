// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelhubchat.h"
#include "modelcenter/modelhub/modelhubwrapper.h"
#include <QLoggingCategory>

AIDAEMON_USE_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

using namespace modelhub;

ModelhubChat::ModelhubChat(const QString &model, QSharedPointer<ModelhubWrapper> wrapper, QObject *parent)
    : UniversalChatInterface(model, parent)
    , m_wrapper(wrapper)
{

}

ModelhubChat::~ModelhubChat()
{

}

QJsonObject ModelhubChat::generate(const QString &content, bool stream, const QList<ChatHistory> &history, const QVariantHash &params)
{
    bool isRun = m_wrapper->ensureRunning();
    if (!isRun) {
        qCWarning(logModelCenter) << "Modelhub service is not running, cannot generate response";
        return {};
    }

    qCDebug(logModelCenter) << "Generating response for content length:" << content.length()
                           << "with history size:" << history.size();
    return UniversalChatInterface::generate(content, stream, history, params);
}

QString ModelhubChat::apiUrl() const
{
    return  m_wrapper->urlPath("/chat/completions");
}
