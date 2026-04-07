// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dataparsertask.h"
#include "tools/textchunker.h"
#include "parsers/parserfactory.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QThread>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(dataParserTask, "aidaemon.vectordb.dataparsertask")

// Data Parser Task Implementation
DataParserTask::DataParserTask(const QString &taskId, const QVariantMap &params)
    : StageTask(taskId, StageBasedTaskManager::ProcessingStage::DataParsing)
{
    m_files = params.value("files").toStringList();

    qCDebug(dataParserTask) << "Creating data parser task:" << taskId 
                           << "with" << m_files.size() << "files"
                           << "chunkSize:" << m_chunkSize
                           << "overlapSize:" << m_overlapSize
                           << "outputFormat:" << m_outputFormat;
}

QVariantMap DataParserTask::getResourceRequirements() const
{
    QVariantMap requirements;
    requirements["cpu"] = 0.8; // High CPU usage
    requirements["memory"] = 512; // MB
    requirements["gpu"] = 0.0; // No GPU needed
    requirements["network"] = 0.1; // Minimal network
    requirements["io"] = 0.6; // Moderate I/O
    return requirements;
}

qreal DataParserTask::getEstimatedDuration() const
{
    // Estimate 30 seconds per document
    return m_files.size() * 30.0;
}

QString DataParserTask::getResourceType() const
{
    return "cpu";
}

bool DataParserTask::initializeExecution()
{
    qCDebug(dataParserTask) << "Initializing data parser task:" << getTaskId();
    setProgress(5);
    return true;
}

QVariantMap DataParserTask::executeStage()
{
    qCDebug(dataParserTask) << "Executing data parser task:" << getTaskId() << "for" << m_files.size() << "files";

    QVariantMap results;
    QVariantList allChunks;
    QStringList processedFiles;
    QStringList failedFiles;
    int totalChunks = 0;

    TextChunker chunker;
    
    // Configure chunker settings from task parameters
    TextChunker::ChunkConfig chunkConfig = chunker.getDefaultConfig();
    chunkConfig.maxSize = m_chunkSize;
    chunkConfig.overlap = m_overlapSize;
    chunkConfig.strategy = TextChunker::ChunkingStrategy::FixedSize;
    chunkConfig.preserveWords = true;
    chunkConfig.preserveSentences = false;
    chunkConfig.minChunkSize = qMin(50, m_chunkSize / 10);  // At least 10% of chunk size
    
    for (int i = 0; i < m_files.size(); ++i) {
        if (checkCancellation()) {
            qCDebug(dataParserTask) << "Task cancelled during processing";
            break;
        }
        
        checkPause();
        
        const QString &filePath = m_files[i];
        qCDebug(dataParserTask) << "Processing file:" << filePath << "(" << (i + 1) << "/" << m_files.size() << ")";
        
        // Check if file exists and is readable
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isReadable()) {
            qCWarning(dataParserTask) << "File not accessible:" << filePath;
            failedFiles.append(filePath);
            continue;
        }
        
        try {
            // Step 1: Parse document to extract text
            QSharedPointer<AbstractFileParser> parser(ParserFactory::instance()->createParserForFile(filePath));
            if (parser.isNull()) {
                qCWarning(dataParserTask) << "No suitable parser found for file:" << filePath;
                failedFiles.append(filePath);
                continue;
            }

            QSharedPointer<ParserResult> parseResult = parser->parseFile(filePath);
            if (parseResult.isNull() || parseResult->status() != 0) {
                QString errorMsg = parseResult.isNull() ? "Parse result is null" : parseResult->errorString();
                qCWarning(dataParserTask) << "Failed to parse document:" << filePath << "Error:" << errorMsg;
                failedFiles.append(filePath);
                continue;
            }
            
            QString extractedText = parseResult->getAllText();
            if (extractedText.isEmpty()) {
                qCWarning(dataParserTask) << "No text content extracted from file:" << filePath;
                failedFiles.append(filePath);
                continue;
            }
            
            qCDebug(dataParserTask) << "Extracted" << extractedText.length() << "characters from" << filePath;
            
            // Step 2: Chunk the extracted text
            QList<TextChunker::TextChunk> textChunks = chunker.chunkText(extractedText, chunkConfig);
            if (textChunks.isEmpty()) {
                qCWarning(dataParserTask) << "No chunks created from file:" << filePath;
                failedFiles.append(filePath);
                continue;
            }
            
            qCDebug(dataParserTask) << "Created" << textChunks.size() << "chunks from" << filePath;
            
            // Step 3: Create chunk objects with metadata
            for (int chunkIndex = 0; chunkIndex < textChunks.size(); ++chunkIndex) {
                const TextChunker::TextChunk &chunk = textChunks[chunkIndex];
                
                QVariantMap chunkData;
                chunkData["text"] = chunk.content;
                chunkData["sourceFile"] = filePath;
                chunkData["chunkIndex"] = chunk.chunkIndex;
                chunkData["totalChunks"] = textChunks.size();
                chunkData["chunkId"] = chunk.id;
                chunkData["startPosition"] = chunk.startPosition;
                chunkData["endPosition"] = chunk.endPosition;

                // Add document metadata
                QFileInfo docInfo(filePath);
                chunkData["document_type"] = docInfo.suffix().toLower();
                chunkData["document_encoding"] = "utf-8";
                chunkData["document_size"] = parseResult->totalSize();
                
                // Add chunk metadata
                for (auto it = chunk.metadata.begin(); it != chunk.metadata.end(); ++it) {
                    chunkData[QString("chunk_%1").arg(it.key())] = it.value();
                }
                
                allChunks.append(chunkData);
            }
            
            totalChunks += textChunks.size();
            processedFiles.append(filePath);
            
        } catch (const std::exception &e) {
            qCCritical(dataParserTask) << "Exception processing file" << filePath << ":" << e.what();
            failedFiles.append(filePath);
        } catch (...) {
            qCCritical(dataParserTask) << "Unknown exception processing file:" << filePath;
            failedFiles.append(filePath);
        }
        
        // Update progress
        int progress = 10 + (80 * (i + 1)) / m_files.size();
        setProgress(progress);
    }
    
    // Prepare results
    results["textChunks"] = allChunks;  // Use textChunks to match validation logic
    results["totalChunks"] = totalChunks;
    results["processedFiles"] = processedFiles;
    results["failedFiles"] = failedFiles;
    results["totalProcessed"] = processedFiles.size();
    results["totalFailed"] = failedFiles.size();
    results["skippedFiles"] = m_files.size() - processedFiles.size();
    results["success"] = processedFiles.size() > 0;  // Success if at least one file processed
    
    // Add processing statistics
    results["chunkSize"] = m_chunkSize;
    results["overlapSize"] = m_overlapSize;
    results["outputFormat"] = m_outputFormat;
    results["averageChunksPerFile"] = processedFiles.size() > 0 ? (double)totalChunks / processedFiles.size() : 0.0;
    
    // Add timing information
    results["processingTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    qCDebug(dataParserTask) << "Data parsing completed. Processed" << processedFiles.size()
                          << "files, failed" << failedFiles.size() 
                          << "files, created" << totalChunks << "chunks";
    
    if (failedFiles.size() > 0) {
        qCWarning(dataParserTask) << "Failed to process files:" << failedFiles;
    }
    
    setProgress(90);
    return results;
}

void DataParserTask::finalizeExecution()
{
    qCDebug(dataParserTask) << "Finalizing data parser task:" << getTaskId();
    setProgress(100);
}

void DataParserTask::cleanupResources()
{
    qCDebug(dataParserTask) << "Cleaning up data parser task resources:" << getTaskId();
}

AIDAEMON_VECTORDB_END_NAMESPACE
