// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "videopropertyparser.h"

#include <QLibrary>
#include <QJsonObject>
#include <QSize>
#include <QUrl>
#include <QDebug>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logIndexCenter)

VideoPropertyParser::VideoPropertyParser(QObject *parent)
    : AbstractPropertyParser(parent)
{
    initLibrary();
}

QList<AbstractPropertyParser::Property> VideoPropertyParser::properties(const QString &file)
{
    auto propertyList = AbstractPropertyParser::properties(file);
    if (propertyList.isEmpty())
        return propertyList;

    QSize resolution;
    qint64 duration = 0;
    if (getMovieInfo(QUrl::fromLocalFile(file), resolution, duration)) {
        if (duration != -1)
            propertyList.append({ "duration", formatTime(duration * 1000), false });

        if (resolution != QSize { -1, -1 })
            propertyList.append({ "resolution", QString("%1*%2").arg(resolution.width()).arg(resolution.height()), false });
    }

    return propertyList;
}

void VideoPropertyParser::initLibrary()
{
#ifdef COMPILE_ON_QT6
    QLibrary imageLib("libimageviewer6.so");
#else
    QLibrary imageLib("libimageviewer.so");
#endif

    if (!imageLib.load()) {
        qCWarning(logIndexCenter) << "Failed to load libimageviewer:" << imageLib.errorString();
        return;
    }

    getMovieInfoFunc = reinterpret_cast<GetMovieInfoFunc>(imageLib.resolve("getMovieInfoByJson"));
    if (!getMovieInfoFunc) {
        qCWarning(logIndexCenter) << "Failed to resolve getMovieInfoByJson function";
    } else {
        qCDebug(logIndexCenter) << "Successfully initialized video property parser library";
    }
}

bool VideoPropertyParser::getMovieInfo(const QUrl &url, QSize &dimension, qint64 &duration)
{
    if (!getMovieInfoFunc) {
        qCWarning(logIndexCenter) << "Movie info function not available";
        return false;
    }

    QJsonObject json;
    getMovieInfoFunc(url, &json);

    int ok = 0;
    if (json.contains("Base")) {
        const QJsonObject &base = json.value("Base").toObject();
        const QJsonValue &durationJson = base.value("Duration");
        if (durationJson.isString()) {
            QString timeStr = durationJson.toString();
            qCDebug(logIndexCenter) << "Parsing duration string:" << timeStr;
            QStringList times = timeStr.split(':');
            if (times.size() == 3) {
                duration = times.at(0).toInt() * 3600 + times.at(1).toInt() * 60 + times.at(2).toInt();
                ok = ok | 1;
            }
        }
        if (!(ok & 1)) {
            duration = -1;
            qCWarning(logIndexCenter) << "Failed to parse video duration";
        }

        const QJsonValue &resolutionValue = base.value("Resolution");
        if (resolutionValue.isString()) {
            QString resolutionStr = resolutionValue.toString();
            qCDebug(logIndexCenter) << "Parsing resolution string:" << resolutionStr;
            QStringList strs = resolutionStr.split('x');
            if (strs.size() == 2) {
                dimension = QSize(strs.at(0).toInt(), strs.at(1).toInt());
                ok = ok | 2;
            }
        }

        if (!(ok & 2)) {
            dimension = QSize(-1, -1);
            qCWarning(logIndexCenter) << "Failed to parse video resolution";
        }
    } else {
        qCWarning(logIndexCenter) << "No base information found in video metadata for:" << url.toString();
        return false;
    }

    return true;
}
