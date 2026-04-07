// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VECTORDB_DEFINE_H
#define VECTORDB_DEFINE_H

#include "aidaemon_global.h"

#include <QString>
#include <QVariantHash>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Extension parameters for vector database operations
 * 
 * This namespace contains constants and helper functions for working with
 * extension parameters in vector database operations.
 */
namespace ExtensionParams
{
    inline constexpr char kModelKey[] = "model" ;
    inline constexpr char kTopkKey[] = "topk";

    /**
     * @brief Extract model from extension parameters
     * @param params Extension parameters
     * @param defaultValue Default value if not specified
     * @return Model identifier
     */
    inline QString getModel(const QVariantHash &params, const QString &defaultValue = QString())
    {
        return params.value(kModelKey, defaultValue).toString();
    }
    
    /**
     * @brief Extract topk from extension parameters
     * @param params Extension parameters
     * @param defaultValue Default value if not specified
     * @return Number of top results
     */
    inline int getTopk(const QVariantHash &params, int defaultValue = 10)
    {
        return params.value(kTopkKey, defaultValue).toInt();
    }
}

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORDB_EXTENSION_DEFINE_H
