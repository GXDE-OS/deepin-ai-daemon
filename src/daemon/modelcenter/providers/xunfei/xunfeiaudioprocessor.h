// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef XUNFEIAUDIOPROCESSOR_H
#define XUNFEIAUDIOPROCESSOR_H

#include "aidaemon_global.h"

#include <QObject>
#include <QByteArray>
#include <QList>

AIDAEMON_BEGIN_NAMESPACE

class XunfeiAudioProcessor : public QObject
{
    Q_OBJECT
public:
    explicit XunfeiAudioProcessor(QObject *parent = nullptr);
    ~XunfeiAudioProcessor();
    
    // Audio format validation
    static bool isValidAudioFile(const QString &filePath);
    static bool isSupportedFormat(const QString &filePath);
    
    // Audio format conversion
    QByteArray convertToPCM16k(const QByteArray &audioData, const QString &sourceFormat = "");
    QByteArray loadAudioFile(const QString &filePath);
    
    // Audio frame processing
    QList<QByteArray> splitToFrames(const QByteArray &audioData, int frameSize = 8000);
    
    // Audio properties
    static int getAudioSampleRate(const QString &filePath);
    static int getAudioChannels(const QString &filePath);
    static QString getAudioFormat(const QString &filePath);
    
    // Frame timing
    static int getFrameInterval() { return 40; } // 40ms interval
    static int getDefaultFrameSize() { return 8000; } // 8KB per frame
    
private:
    bool convertToRawPCM(const QString &inputFile, const QString &outputFile);
    QByteArray readRawPCM(const QString &filePath);
    
private:
    static const QStringList supportedFormats;
};

AIDAEMON_END_NAMESPACE

#endif // XUNFEIAUDIOPROCESSOR_H 
