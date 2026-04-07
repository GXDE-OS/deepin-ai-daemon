// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "aitypes.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logCommon, "ai.daemon.common")

AIDAEMON_USE_NAMESPACE

QString AITypes::serviceTypeToString(ServiceType type)
{
    switch (type) {
        // NLP tasks
        case ServiceType::Chat: return "Chat";
        case ServiceType::FunctionCalling: return "FunctionCalling";

        // Speech tasks
        case ServiceType::TextToSpeech: return "TextToSpeech";
        case ServiceType::SpeechToText: return "SpeechToText";

        // Vision tasks
        case ServiceType::ImageRecognition: return "ImageRecognition";
        case ServiceType::OCR: return "OCR";
        case ServiceType::ImageGeneration: return "ImageGeneration";
        case ServiceType::ImageEditing: return "ImageEditing";
        case ServiceType::ImageClassification: return "ImageClassification";

        // Embedding tasks
        case ServiceType::TextEmbedding: return "TextEmbedding";
        case ServiceType::VectorSearch: return "VectorSearch";
        case ServiceType::SemanticSearch: return "SemanticSearch";

        default: return "Unknown";
    }
}

AITypes::ServiceType AITypes::stringToServiceType(const QString& str)
{
    // Normalize the input string to handle different formats
    QString normalizedStr = str;
    
    // Convert to lowercase and replace underscores with camelCase
    if (str.contains('_')) {
        // Handle underscore format (e.g., "text_to_speech" -> "TextToSpeech")
        QStringList parts = str.split('_');
        normalizedStr.clear();
        for (const QString& part : parts) {
            if (!part.isEmpty()) {
                normalizedStr += part.at(0).toUpper() + part.mid(1).toLower();
            }
        }
    } else {
        normalizedStr = str;
    }
    
    // Create static map for efficient lookup (using normalized camelCase keys)
    static const QHash<QString, ServiceType> typeMap = {
        // NLP tasks
        {"Chat", ServiceType::Chat},
        {"FunctionCalling", ServiceType::FunctionCalling},
        
        // Speech tasks
        {"TextToSpeech", ServiceType::TextToSpeech},
        {"SpeechToText", ServiceType::SpeechToText},
        
        // Vision tasks
        {"ImageRecognition", ServiceType::ImageRecognition},
        {"OCR", ServiceType::OCR},
        {"ImageGeneration", ServiceType::ImageGeneration},
        {"ImageEditing", ServiceType::ImageEditing},
        {"ImageClassification", ServiceType::ImageClassification},
        
        // Embedding tasks
        {"TextEmbedding", ServiceType::TextEmbedding},
        {"VectorSearch", ServiceType::VectorSearch},
        {"SemanticSearch", ServiceType::SemanticSearch}
    };
    
    auto it = typeMap.find(normalizedStr);
    if (it != typeMap.end()) {
        return it.value();
    }
    
    qCWarning(logCommon) << "Unknown service type string:" << str << "(normalized:" << normalizedStr << ")";
    return ServiceType::Chat; // Default fallback
}

AITypes::ServiceGroup AITypes::getServiceGroup(ServiceType type)
{
    int typeValue = static_cast<int>(type);
    
    if (typeValue >= 100 && typeValue < 200) {
        return ServiceGroup::NLP;
    } else if (typeValue >= 200 && typeValue < 300) {
        return ServiceGroup::Speech;
    } else if (typeValue >= 300 && typeValue < 400) {
        return ServiceGroup::Vision;
    } else if (typeValue >= 400 && typeValue < 500) {
        return ServiceGroup::Embedding;
    }
    
    return ServiceGroup::NLP; // Default fallback
}

QString AITypes::serviceGroupToString(ServiceGroup group)
{
    switch (group) {
        case ServiceGroup::NLP: return "NLP";
        case ServiceGroup::Speech: return "Speech";
        case ServiceGroup::Vision: return "Vision";
        case ServiceGroup::Embedding: return "Embedding";
        default: return "Unknown";
    }
}

bool AITypes::isValidServiceType(int type)
{
    return (type >= 100 && type < 200) ||   // NLP
           (type >= 200 && type < 300) ||   // Speech
           (type >= 300 && type < 400) ||   // Vision
           (type >= 400 && type < 500);     // Embedding
}

QList<AITypes::ServiceType> AITypes::getServiceTypesInGroup(ServiceGroup group)
{
    QList<ServiceType> types;
    
    switch (group) {
        case ServiceGroup::NLP:
            types << ServiceType::Chat
                  << ServiceType::FunctionCalling;
            break;
            
        case ServiceGroup::Speech:
            types << ServiceType::TextToSpeech
                  << ServiceType::SpeechToText;
            break;
            
        case ServiceGroup::Vision:
            types << ServiceType::ImageRecognition
                  << ServiceType::OCR
                  << ServiceType::ImageGeneration
                  << ServiceType::ImageEditing
                  << ServiceType::ImageClassification;
            break;
            
        case ServiceGroup::Embedding:
            types << ServiceType::TextEmbedding
                  << ServiceType::VectorSearch
                  << ServiceType::SemanticSearch;
            break;
    }
    
    return types;
} 
