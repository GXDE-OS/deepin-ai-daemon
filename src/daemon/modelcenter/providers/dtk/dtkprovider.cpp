// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dtkprovider.h"
#include "dtkocr.h"

#include <QLoggingCategory>

AIDAEMON_USE_NAMESPACE

Q_LOGGING_CATEGORY(logDTKProvider, "ai.daemon.dtk.provider")

DTKProvider *DTKProvider::create()
{
    return new DTKProvider();
}

DTKProvider::DTKProvider()
    : ModelProvider()
{
    qCDebug(logDTKProvider) << "Creating DTK provider";
}

DTKProvider::~DTKProvider()
{
    qCDebug(logDTKProvider) << "DTK provider destroyed";
}

QString DTKProvider::name()
{
    return providerName();
}

AbstractInterface *DTKProvider::createInterface(InterfaceType type)
{
    QVariantHash modelInfo;
    switch (type) {
    case OCR:
        // For DTK provider, we use default configuration since it's a local provider
        modelInfo = QVariantHash();
        break;
    default:
        qCWarning(logDTKProvider) << "Unsupported interface type:" << type << "for DTK provider";
        break;
    }

    return createInterface(type, modelInfo);
}

AbstractInterface *DTKProvider::createInterface(InterfaceType type, const QVariantHash &modelInfo)
{
    AbstractInterface *ret = nullptr;
    switch (type) {
    case OCR:
        ret = createOCR(modelInfo);
        break;
    default:
        qCWarning(logDTKProvider) << "Unsupported interface type:" << type << "for DTK provider";
        break;
    }

    return ret;
}

OCRInterface *DTKProvider::createOCR(const QVariantHash &modelInfo)
{
    // For DTK provider, we can use model from modelInfo or default
    QString model = modelInfo.value(kModelInfoModel).toString();
    if (model.isEmpty()) {
        model = "dtk-ocr-default";
    }
    
    qCInfo(logDTKProvider) << "Creating new DTK OCR interface with model:" << model;
    auto ocrInterface = new DTKOCRInterface(model);
    
    // Initialize the interface
    if (!ocrInterface->initialize()) {
        qCWarning(logDTKProvider) << "Failed to initialize DTK OCR interface";
        delete ocrInterface;
        return nullptr;
    }
    
    return ocrInterface;
}
