// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DOUBALPROVIDER_H
#define DOUBALPROVIDER_H

#include "modelcenter/providers/modelprovider.h"

AIDAEMON_BEGIN_NAMESPACE
class ChatInterface;
class ImageRecognitionInterface;
class EmbeddingInterface;
class DouBaoProviderPrivate;
class DouBaoProvider : public ModelProvider
{
    friend class DouBaoProviderPrivate;
public:
    static inline QString providerName() {
        return "doubao";
    }
    static DouBaoProvider *create();

    DouBaoProvider();
    ~DouBaoProvider() override;
public:
    QString name() override;
    AbstractInterface *createInterface(InterfaceType type) override;
    AbstractInterface *createInterface(InterfaceType type, const QVariantHash &modelInfo) override;
protected:
    ChatInterface *createChat(const QVariantHash &modelInfo);
    ImageRecognitionInterface *createImageRecognition(const QVariantHash &modelInfo);
    EmbeddingInterface *createEmbedding(const QVariantHash &modelInfo);
private:
    DouBaoProviderPrivate *d = nullptr;
};

AIDAEMON_END_NAMESPACE

#endif // DouBaoPROVIDER_H
