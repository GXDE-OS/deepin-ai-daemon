// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "textchunker.h"
#include "qt5_6_compatible.h"

#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QUuid>
#include <QtCore/QRegularExpression>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(textChunker, "aidaemon.vectordb.textchunker")

TextChunker::TextChunker(QObject *parent)
    : QObject(parent)
{
    qCDebug(textChunker) << "Creating text chunker";
}

TextChunker::~TextChunker()
{
    qCDebug(textChunker) << "Destroying text chunker";
}

QList<TextChunker::TextChunk> TextChunker::chunkText(const QString &text, const ChunkConfig &config)
{
    qCDebug(textChunker) << "Chunking text of length:" << text.length() << "with strategy:" << static_cast<int>(config.strategy);
    
    if (text.isEmpty()) {
        return {};
    }
    
    if (!validateConfig(config)) {
        qCWarning(textChunker) << "Invalid chunking configuration";
        return {};
    }
    
    QList<TextChunk> chunks;
    
    switch (config.strategy) {
    case ChunkingStrategy::FixedSize:
        chunks = chunkByFixedSize(text, config);
        break;
    case ChunkingStrategy::Sentence:
        chunks = chunkBySentence(text, config);
        break;
    case ChunkingStrategy::Paragraph:
        chunks = chunkByParagraph(text, config);
        break;
    case ChunkingStrategy::Semantic:
        chunks = chunkBySemantic(text, config);
        break;
    case ChunkingStrategy::Sliding:
        chunks = chunkBySliding(text, config);
        break;
    }
    
    qCDebug(textChunker) << "Created" << chunks.size() << "chunks";
    
    return chunks;
}

QList<TextChunker::TextChunk> TextChunker::chunkText(const QString &text)
{
    return chunkText(text, getDefaultConfig());
}

QVariantList TextChunker::chunkText(const QString &text, const QVariantMap &config)
{
    ChunkConfig chunkConfig = variantMapToConfig(config);
    QList<TextChunk> chunks = chunkText(text, chunkConfig);
    
    QVariantList result;
    for (const TextChunk &chunk : chunks) {
        result.append(chunkToVariantMap(chunk));
    }
    
    return result;
}

TextChunker::ChunkConfig TextChunker::getDefaultConfig() const
{
    ChunkConfig config;
    config.strategy = ChunkingStrategy::FixedSize;
    config.maxSize = 1000;
    config.overlap = 100;
    config.preserveWords = true;
    config.preserveSentences = false;
    config.separator = "\n\n";
    config.minChunkSize = 50;
    
    return config;
}

bool TextChunker::validateConfig(const ChunkConfig &config) const
{
    if (config.maxSize <= 0 || config.maxSize > 100000) {
        qCWarning(textChunker) << "Invalid max size:" << config.maxSize;
        return false;
    }
    
    if (config.overlap < 0 || config.overlap >= config.maxSize) {
        qCWarning(textChunker) << "Invalid overlap:" << config.overlap;
        return false;
    }
    
    if (config.minChunkSize <= 0 || config.minChunkSize > config.maxSize) {
        qCWarning(textChunker) << "Invalid min chunk size:" << config.minChunkSize;
        return false;
    }
    
    // Additional check: ensure effective chunk size is positive to prevent infinite loops
    int effectiveChunkSize = config.maxSize - config.overlap;
    if (effectiveChunkSize <= 0) {
        qCWarning(textChunker) << "Invalid configuration: effective chunk size (maxSize - overlap) must be positive. maxSize:" << config.maxSize << "overlap:" << config.overlap;
        return false;
    }
    
    return true;
}

int TextChunker::estimateChunkCount(const QString &text, const ChunkConfig &config) const
{
    if (text.isEmpty() || config.maxSize <= 0) {
        return 0;
    }
    
    int effectiveChunkSize = config.maxSize - config.overlap;
    if (effectiveChunkSize <= 0) {
        effectiveChunkSize = config.maxSize;
    }
    
    return (text.length() + effectiveChunkSize - 1) / effectiveChunkSize;
}

int TextChunker::estimateChunkCount(const QString &text) const
{
    return estimateChunkCount(text, getDefaultConfig());
}

QVariantMap TextChunker::getChunkingStatistics(const QString &text, const ChunkConfig &config) const
{
    QVariantMap stats;
    
    stats["textLength"] = text.length();
    stats["estimatedChunks"] = estimateChunkCount(text, config);
    stats["strategy"] = static_cast<int>(config.strategy);
    stats["maxSize"] = config.maxSize;
    stats["overlap"] = config.overlap;
    stats["minChunkSize"] = config.minChunkSize;
    
    // Count sentences and paragraphs
    QStringList sentences = splitIntoSentences(text);
    QStringList paragraphs = splitIntoParagraphs(text, config.separator);
    
    stats["sentenceCount"] = sentences.size();
    stats["paragraphCount"] = paragraphs.size();
    stats["wordCount"] = text.split(QRegularExpression("\\s+"), QtStringSkipEmptyParts).size();
    
    return stats;
}

QVariantMap TextChunker::getChunkingStatistics(const QString &text) const
{
    return getChunkingStatistics(text, getDefaultConfig());
}

QList<TextChunker::TextChunk> TextChunker::chunkByFixedSize(const QString &text, const ChunkConfig &config)
{
    QList<TextChunk> chunks;
    int textLength = text.length();
    int chunkIndex = 0;
    int startPos = 0;
    
    while (startPos < textLength) {
        
        int endPos = qMin(startPos + config.maxSize, textLength);
        
        // Find appropriate break position
        if (endPos < textLength) {
            endPos = findBreakPosition(text, startPos, endPos, config);
        }
        
        // Ensure minimum chunk size
        if (endPos - startPos < config.minChunkSize && endPos < textLength) {
            endPos = qMin(startPos + config.minChunkSize, textLength);
        }
        
        QString chunkContent = text.mid(startPos, endPos - startPos).trimmed();
        if (!chunkContent.isEmpty()) {
            QVariantMap metadata;
            metadata["strategy"] = "fixed_size";
            metadata["originalStart"] = startPos;
            metadata["originalEnd"] = endPos;
            
            TextChunk chunk = createChunk(chunkContent, startPos, endPos, chunkIndex++, metadata);
            chunks.append(chunk);
        }
        
        // Move start position with overlap
        int newStartPos = endPos - config.overlap;
        
        // Ensure we make forward progress and don't create invalid positions
        if (newStartPos <= startPos) {
            // If overlap is too large, move by at least 1 character to avoid infinite loop
            startPos = startPos + qMax(1, config.maxSize - config.overlap);
        } else {
            startPos = newStartPos;
        }
        
        // Ensure startPos doesn't exceed text boundaries
        if (startPos >= textLength) {
            break;
        }
    }
    
    return chunks;
}

QList<TextChunker::TextChunk> TextChunker::chunkBySentence(const QString &text, const ChunkConfig &config)
{
    QList<TextChunk> chunks;
    QStringList sentences = splitIntoSentences(text);
    
    if (sentences.isEmpty()) {
        return chunks;
    }
    
    QString currentChunk;
    int chunkIndex = 0;
    int startPos = 0;
    int currentPos = 0;
    
    for (int i = 0; i < sentences.size(); ++i) {
        const QString &sentence = sentences[i];
        QString testChunk = currentChunk.isEmpty() ? sentence : currentChunk + " " + sentence;
        
        if (testChunk.length() > config.maxSize && !currentChunk.isEmpty()) {
            // Create chunk from current content
            QVariantMap metadata;
            metadata["strategy"] = "sentence";
            metadata["sentenceCount"] = currentChunk.split('.', QtStringSkipEmptyParts).size();
            
            TextChunk chunk = createChunk(currentChunk.trimmed(), startPos, currentPos, chunkIndex++, metadata);
            chunks.append(chunk);
            
            // Start new chunk
            currentChunk = sentence;
            startPos = currentPos;
        } else {
            currentChunk = testChunk;
        }
        
        currentPos += sentence.length() + 1; // +1 for space/separator
    }
    
    // Add remaining content
    if (!currentChunk.isEmpty()) {
        QVariantMap metadata;
        metadata["strategy"] = "sentence";
        metadata["sentenceCount"] = currentChunk.split('.', QtStringSkipEmptyParts).size();
        
        TextChunk chunk = createChunk(currentChunk.trimmed(), startPos, text.length(), chunkIndex, metadata);
        chunks.append(chunk);
    }
    
    return chunks;
}

QList<TextChunker::TextChunk> TextChunker::chunkByParagraph(const QString &text, const ChunkConfig &config)
{
    QList<TextChunk> chunks;
    QStringList paragraphs = splitIntoParagraphs(text, config.separator);
    
    if (paragraphs.isEmpty()) {
        return chunks;
    }
    
    QString currentChunk;
    int chunkIndex = 0;
    int startPos = 0;
    int currentPos = 0;
    
    for (int i = 0; i < paragraphs.size(); ++i) {
        const QString &paragraph = paragraphs[i];
        QString testChunk = currentChunk.isEmpty() ? paragraph : currentChunk + config.separator + paragraph;
        
        if (testChunk.length() > config.maxSize && !currentChunk.isEmpty()) {
            // Create chunk from current content
            QVariantMap metadata;
            metadata["strategy"] = "paragraph";
            metadata["paragraphCount"] = currentChunk.split(config.separator, QtStringSkipEmptyParts).size();
            
            TextChunk chunk = createChunk(currentChunk.trimmed(), startPos, currentPos, chunkIndex++, metadata);
            chunks.append(chunk);
            
            // Start new chunk
            currentChunk = paragraph;
            startPos = currentPos;
        } else {
            currentChunk = testChunk;
        }
        
        currentPos += paragraph.length() + config.separator.length();
    }
    
    // Add remaining content
    if (!currentChunk.isEmpty()) {
        QVariantMap metadata;
        metadata["strategy"] = "paragraph";
        metadata["paragraphCount"] = currentChunk.split(config.separator, QtStringSkipEmptyParts).size();
        
        TextChunk chunk = createChunk(currentChunk.trimmed(), startPos, text.length(), chunkIndex, metadata);
        chunks.append(chunk);
    }
    
    return chunks;
}

QList<TextChunker::TextChunk> TextChunker::chunkBySemantic(const QString &text, const ChunkConfig &config)
{
    // Semantic chunking requires advanced NLP techniques
    // For now, fall back to sentence-based chunking
    qCDebug(textChunker) << "Semantic chunking not fully implemented, using sentence-based chunking";
    return chunkBySentence(text, config);
}

QList<TextChunker::TextChunk> TextChunker::chunkBySliding(const QString &text, const ChunkConfig &config)
{
    QList<TextChunk> chunks;
    int textLength = text.length();
    int chunkIndex = 0;
    int startPos = 0;
    int stepSize = config.maxSize - config.overlap;
    
    if (stepSize <= 0) {
        stepSize = config.maxSize / 2; // Default to 50% overlap
    }
    
    while (startPos < textLength) {
        
        int endPos = qMin(startPos + config.maxSize, textLength);
        
        // Find appropriate break position
        if (endPos < textLength) {
            endPos = findBreakPosition(text, startPos, endPos, config);
        }
        
        QString chunkContent = text.mid(startPos, endPos - startPos).trimmed();
        if (!chunkContent.isEmpty() && chunkContent.length() >= config.minChunkSize) {
            QVariantMap metadata;
            metadata["strategy"] = "sliding";
            metadata["overlap"] = config.overlap;
            metadata["stepSize"] = stepSize;
            
            TextChunk chunk = createChunk(chunkContent, startPos, endPos, chunkIndex++, metadata);
            chunks.append(chunk);
        }
        
        startPos += stepSize;
    }
    
    return chunks;
}

QStringList TextChunker::splitIntoSentences(const QString &text) const
{
    // Simple sentence splitting - in a real implementation, use a proper NLP library
    QRegularExpression sentenceRegex(R"([.!?]+\s+)");
    QStringList sentences = text.split(sentenceRegex, QtStringSkipEmptyParts);
    
    // Clean up sentences
    for (QString &sentence : sentences) {
        sentence = sentence.trimmed();
    }
    
    sentences.removeAll(QString());
    return sentences;
}

QStringList TextChunker::splitIntoParagraphs(const QString &text, const QString &separator) const
{
    QStringList paragraphs = text.split(separator, QtStringSkipEmptyParts);
    
    // Clean up paragraphs
    for (QString &paragraph : paragraphs) {
        paragraph = paragraph.trimmed();
    }
    
    paragraphs.removeAll(QString());
    return paragraphs;
}

QString TextChunker::generateChunkId(int index) const
{
    return QString("chunk_%1_%2").arg(index).arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
}

TextChunker::TextChunk TextChunker::createChunk(const QString &content, int startPos, int endPos, int index, const QVariantMap &metadata) const
{
    TextChunk chunk;
    chunk.id = generateChunkId(index);
    chunk.content = content;
    chunk.startPosition = startPos;
    chunk.endPosition = endPos;
    chunk.chunkIndex = index;
    chunk.metadata = metadata;
    chunk.metadata["length"] = content.length();
    chunk.metadata["wordCount"] = content.split(QRegularExpression("\\s+"), QtStringSkipEmptyParts).size();
    
    return chunk;
}

bool TextChunker::shouldBreakAtPosition(const QString &text, int position, const ChunkConfig &config) const
{
    if (position >= text.length()) {
        return true;
    }
    
    QChar ch = text.at(position);
    
    if (config.preserveWords && ch.isLetter()) {
        return false;
    }
    
    if (config.preserveSentences && ch != '.' && ch != '!' && ch != '?') {
        return false;
    }
    
    return ch.isSpace() || ch.isPunct();
}

int TextChunker::findBreakPosition(const QString &text, int startPos, int maxPos, const ChunkConfig &config) const
{
    // Look backwards from maxPos to find a good break point
    for (int pos = maxPos - 1; pos > startPos; --pos) {
        if (shouldBreakAtPosition(text, pos, config)) {
            return pos + 1;
        }
    }
    
    // If no good break point found, use maxPos
    return maxPos;
}

QVariantMap TextChunker::chunkToVariantMap(const TextChunk &chunk) const
{
    QVariantMap map;
    map["id"] = chunk.id;
    map["content"] = chunk.content;
    map["startPosition"] = chunk.startPosition;
    map["endPosition"] = chunk.endPosition;
    map["chunkIndex"] = chunk.chunkIndex;
    map["metadata"] = chunk.metadata;
    
    return map;
}

TextChunker::ChunkConfig TextChunker::variantMapToConfig(const QVariantMap &configMap) const
{
    ChunkConfig config = getDefaultConfig();
    
    if (configMap.contains("strategy")) {
        config.strategy = static_cast<ChunkingStrategy>(configMap.value("strategy").toInt());
    }
    
    if (configMap.contains("maxSize")) {
        config.maxSize = configMap.value("maxSize").toInt();
    }
    
    if (configMap.contains("overlap")) {
        config.overlap = configMap.value("overlap").toInt();
    }
    
    if (configMap.contains("preserveWords")) {
        config.preserveWords = configMap.value("preserveWords").toBool();
    }
    
    if (configMap.contains("preserveSentences")) {
        config.preserveSentences = configMap.value("preserveSentences").toBool();
    }
    
    if (configMap.contains("separator")) {
        config.separator = configMap.value("separator").toString();
    }
    
    if (configMap.contains("minChunkSize")) {
        config.minChunkSize = configMap.value("minChunkSize").toInt();
    }
    
    return config;
}

AIDAEMON_VECTORDB_END_NAMESPACE
