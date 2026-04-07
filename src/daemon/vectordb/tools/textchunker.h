// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TEXTCHUNKER_H
#define TEXTCHUNKER_H

#include "aidaemon_global.h"

#include <QtCore/QObject>
#include <QtCore/QVariantMap>
#include <QtCore/QVariantList>
#include <QtCore/QStringList>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

/**
 * @brief Text chunking utility for splitting documents into smaller segments
 * 
 * Provides various chunking strategies for breaking down large text documents
 * into manageable chunks suitable for vector embedding and indexing.
 */
class TextChunker : public QObject
{
    Q_OBJECT

public:
    enum class ChunkingStrategy {
        FixedSize = 1,      // Fixed character/word count
        Sentence = 2,       // Sentence-based chunking
        Paragraph = 3,      // Paragraph-based chunking
        Semantic = 4,       // Semantic similarity-based
        Sliding = 5         // Sliding window with overlap
    };
    Q_ENUM(ChunkingStrategy)

    struct ChunkConfig {
        ChunkingStrategy strategy = ChunkingStrategy::FixedSize;
        int maxSize = 1000;         // Maximum chunk size (characters or words)
        int overlap = 100;          // Overlap between chunks
        bool preserveWords = true;  // Don't break words
        bool preserveSentences = false; // Don't break sentences
        QString separator = "\n\n"; // Paragraph separator
        int minChunkSize = 50;      // Minimum chunk size
    };

    struct TextChunk {
        QString id;
        QString content;
        int startPosition = 0;
        int endPosition = 0;
        int chunkIndex = 0;
        QVariantMap metadata;
    };

    explicit TextChunker(QObject *parent = nullptr);
    ~TextChunker();

    /**
     * @brief Chunk text using specified configuration
     * @param text Text to chunk
     * @param config Chunking configuration
     * @return List of text chunks
     */
    QList<TextChunk> chunkText(const QString &text, const ChunkConfig &config);
    
    /**
     * @brief Chunk text using default configuration
     * @param text Text to chunk
     * @return List of text chunks
     */
    QList<TextChunk> chunkText(const QString &text);

    /**
     * @brief Chunk text with QVariantMap configuration
     * @param text Text to chunk
     * @param config Configuration as QVariantMap
     * @return List of chunks as QVariantList
     */
    QVariantList chunkText(const QString &text, const QVariantMap &config);

    /**
     * @brief Get default chunking configuration
     * @return Default configuration
     */
    ChunkConfig getDefaultConfig() const;

    /**
     * @brief Validate chunking configuration
     * @param config Configuration to validate
     * @return true if valid, false otherwise
     */
    bool validateConfig(const ChunkConfig &config) const;

    /**
     * @brief Estimate number of chunks for given text and config
     * @param text Text to analyze
     * @param config Chunking configuration
     * @return Estimated number of chunks
     */
    int estimateChunkCount(const QString &text, const ChunkConfig &config) const;
    
    /**
     * @brief Estimate number of chunks for given text using default config
     * @param text Text to analyze
     * @return Estimated number of chunks
     */
    int estimateChunkCount(const QString &text) const;

    /**
     * @brief Get chunking statistics for text
     * @param text Text to analyze
     * @param config Chunking configuration
     * @return Statistics as QVariantMap
     */
    QVariantMap getChunkingStatistics(const QString &text, const ChunkConfig &config) const;
    
    /**
     * @brief Get chunking statistics for text using default config
     * @param text Text to analyze
     * @return Statistics as QVariantMap
     */
    QVariantMap getChunkingStatistics(const QString &text) const;

    // Note: Signals removed - all operations are now synchronous

private:
    // Chunking strategy implementations
    QList<TextChunk> chunkByFixedSize(const QString &text, const ChunkConfig &config);
    QList<TextChunk> chunkBySentence(const QString &text, const ChunkConfig &config);
    QList<TextChunk> chunkByParagraph(const QString &text, const ChunkConfig &config);
    QList<TextChunk> chunkBySemantic(const QString &text, const ChunkConfig &config);
    QList<TextChunk> chunkBySliding(const QString &text, const ChunkConfig &config);

    // Helper methods
    QStringList splitIntoSentences(const QString &text) const;
    QStringList splitIntoParagraphs(const QString &text, const QString &separator) const;
    QString generateChunkId(int index) const;
    TextChunk createChunk(const QString &content, int startPos, int endPos, int index, const QVariantMap &metadata = QVariantMap()) const;
    bool shouldBreakAtPosition(const QString &text, int position, const ChunkConfig &config) const;
    int findBreakPosition(const QString &text, int startPos, int maxPos, const ChunkConfig &config) const;
    QVariantMap chunkToVariantMap(const TextChunk &chunk) const;
    ChunkConfig variantMapToConfig(const QVariantMap &configMap) const;
};

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // TEXTCHUNKER_H
