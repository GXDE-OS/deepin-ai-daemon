// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xunfeiaudioprocessor.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(logModelCenter)

AIDAEMON_USE_NAMESPACE

const QStringList XunfeiAudioProcessor::supportedFormats = {
    "wav", "mp3", "flac", "aac", "ogg", "m4a", "pcm"
};

XunfeiAudioProcessor::XunfeiAudioProcessor(QObject *parent)
    : QObject(parent)
{
}

XunfeiAudioProcessor::~XunfeiAudioProcessor()
{
}

bool XunfeiAudioProcessor::isValidAudioFile(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.exists() && fileInfo.isReadable() && fileInfo.size() > 0;
}

bool XunfeiAudioProcessor::isSupportedFormat(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    return supportedFormats.contains(suffix);
}

QByteArray XunfeiAudioProcessor::convertToPCM16k(const QByteArray &audioData, const QString &sourceFormat)
{
    // For now, assume the input is already in the correct format
    // In a production environment, you would use audio libraries like FFmpeg
    // or GStreamer to perform actual format conversion
    
    if (sourceFormat.toLower() == "pcm" || sourceFormat.isEmpty()) {
        return audioData;
    }
    
    // TODO: Implement actual audio format conversion
    qCWarning(logModelCenter) << "Audio format conversion not implemented for format:" << sourceFormat;
    return audioData;
}

QByteArray XunfeiAudioProcessor::loadAudioFile(const QString &filePath)
{
    if (!isValidAudioFile(filePath)) {
        qCWarning(logModelCenter) << "Invalid audio file:" << filePath;
        return QByteArray();
    }
    
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    
    if (suffix == "pcm") {
        // Direct read for PCM files
        return readRawPCM(filePath);
    } else if (isSupportedFormat(filePath)) {
        // Convert other formats to PCM using external tools
        QTemporaryFile tempFile;
        if (tempFile.open()) {
            QString tempPath = tempFile.fileName() + ".pcm";
            tempFile.close();
            
            if (convertToRawPCM(filePath, tempPath)) {
                QByteArray result = readRawPCM(tempPath);
                QFile::remove(tempPath);
                return result;
            }
        }
    }
    
    qCWarning(logModelCenter) << "Failed to load audio file:" << filePath;
    return QByteArray();
}

QList<QByteArray> XunfeiAudioProcessor::splitToFrames(const QByteArray &audioData, int frameSize)
{
    QList<QByteArray> frames;
    
    if (audioData.isEmpty()) {
        return frames;
    }
    
    int totalSize = audioData.size();
    int offset = 0;
    
    while (offset < totalSize) {
        int currentFrameSize = qMin(frameSize, totalSize - offset);
        QByteArray frame = audioData.mid(offset, currentFrameSize);
        frames.append(frame);
        offset += currentFrameSize;
    }
    
    qCDebug(logModelCenter) << "Split audio data into" << frames.size() << "frames";
    return frames;
}

int XunfeiAudioProcessor::getAudioSampleRate(const QString &filePath)
{
    // Use ffprobe to get audio properties
    QProcess process;
    QStringList arguments;
    arguments << "-v" << "quiet"
              << "-print_format" << "json"
              << "-show_streams"
              << "-select_streams" << "a:0"
              << filePath;
    
    process.start("ffprobe", arguments);
    if (process.waitForFinished(5000)) {
        QByteArray output = process.readAllStandardOutput();
        // Parse JSON to extract sample_rate
        // For simplicity, return default value
        return 16000;
    }
    
    return 16000; // Default sample rate
}

int XunfeiAudioProcessor::getAudioChannels(const QString &filePath)
{
    // Similar to sample rate detection
    Q_UNUSED(filePath)
    return 1; // Default to mono
}

QString XunfeiAudioProcessor::getAudioFormat(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower();
}

bool XunfeiAudioProcessor::convertToRawPCM(const QString &inputFile, const QString &outputFile)
{
    // Use ffmpeg to convert audio to raw PCM 16kHz mono
    QProcess process;
    QStringList arguments;
    arguments << "-i" << inputFile
              << "-ar" << "16000"      // Sample rate 16kHz
              << "-ac" << "1"          // Mono channel
              << "-f" << "s16le"       // 16-bit little-endian PCM
              << "-y"                  // Overwrite output file
              << outputFile;
    
    process.start("ffmpeg", arguments);
    if (process.waitForFinished(30000)) { // 30 seconds timeout
        if (process.exitCode() == 0) {
            qCDebug(logModelCenter) << "Successfully converted audio file:" << inputFile;
            return true;
        } else {
            qCWarning(logModelCenter) << "FFmpeg conversion failed:" 
                                      << process.readAllStandardError();
        }
    } else {
        qCWarning(logModelCenter) << "FFmpeg conversion timeout for file:" << inputFile;
    }
    
    return false;
}

QByteArray XunfeiAudioProcessor::readRawPCM(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logModelCenter) << "Failed to open PCM file:" << filePath;
        return QByteArray();
    }
    
    QByteArray data = file.readAll();
    qCDebug(logModelCenter) << "Loaded PCM audio data, size:" << data.size() << "bytes";
    return data;
}
