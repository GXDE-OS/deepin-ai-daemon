// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef IMAGEPROPERTYPARSER_H
#define IMAGEPROPERTYPARSER_H

#include "abstractpropertyparser.h"

#include <QMutex>
#include <QSharedPointer>

class QTimer;
namespace Dtk {
namespace Ocr {
class DOcr;
}
}

class ImagePropertyParser : public AbstractPropertyParser
{
public:
    explicit ImagePropertyParser(QObject *parent = nullptr);
    ~ImagePropertyParser();
    virtual QList<Property> properties(const QString &file) override;
private:
    QSharedPointer<Dtk::Ocr::DOcr> m_ocr;
    QMutex m_mtx;
    //QTimer *m_timer = nullptr;
};

#endif   //IMAGEPROPERTYPARSER_H
