// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imagepropertyparser.h"

#include <DOcr>

#include <QImage>

#include <QThread>
#include <QDebug>
#include <QCoreApplication>
#include <QTimer>

DOCR_USE_NAMESPACE

static constexpr int kImageSizeLimit = 1024;
static DOcr *kOcr = nullptr;


ImagePropertyParser::ImagePropertyParser(QObject *parent)
    : AbstractPropertyParser(parent)
{

}

ImagePropertyParser::~ImagePropertyParser()
{

}

QList<AbstractPropertyParser::Property> ImagePropertyParser::properties(const QString &file)
{
    auto propertyList = AbstractPropertyParser::properties(file);
    if (propertyList.isEmpty())
        return propertyList;

    QImage image(file);
    propertyList.append({ "resolution", QString("%1*%2").arg(image.width()).arg(image.height()), false });

    // 高分辨率图片会导致ocr内存暴涨
    if (image.width() > kImageSizeLimit || image.height() > kImageSizeLimit)
        image = image.width() > image.height()
                ? image.scaledToWidth(kImageSizeLimit, Qt::SmoothTransformation)
                : image.scaledToHeight(kImageSizeLimit, Qt::SmoothTransformation);

    {
        QMutexLocker lk(&m_mtx);
        if (m_ocr.isNull()) {
            m_ocr.reset(new DOcr);
            m_ocr->loadDefaultPlugin();
            m_ocr->setLanguage("zh-Hans_en");
//            m_timer = new QTimer;
//            m_timer->moveToThread(qApp->thread());
//            m_timer->setInterval(60 * 1000);
//            m_timer->setSingleShot(true);
//            QObject::connect(m_timer, &QTimer::timeout, this, [this](){
//                qDebug() << "relase ocr." << this;
//                QMutexLocker lk(&m_mtx);
//                m_ocr.clear();
//                m_timer->deleteLater();
//                m_timer = nullptr;
//            }, Qt::DirectConnection);
        }

        //QMetaObject::invokeMethod(m_timer, "start");
        m_ocr->setImage(image);
        if (m_ocr->analyze()) {
            const auto &result = m_ocr->simpleResult();
            if (!result.isEmpty()) {
                propertyList.append({ "contents", result, true });
            }
        }
    }

    return propertyList;
}
