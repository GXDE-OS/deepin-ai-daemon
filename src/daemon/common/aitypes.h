// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef AITYPES_H
#define AITYPES_H

#include "aidaemon_global.h"
#include <QString>

AIDAEMON_BEGIN_NAMESPACE

namespace AITypes {
    // Unified AI service type definition
    // Used by both SessionManager and TaskManager to avoid duplication
    // Matches exactly with original TaskManager TaskType enum
    enum class ServiceType {
        // NLP tasks (100-199)
        Chat = 100,              // Text conversation
        FunctionCalling = 101,       // Function calling

        // Speech tasks (200-299)
        TextToSpeech = 200,          // Text to speech (TTS)
        SpeechToText = 201,          // Speech to text (ASR)

        // Vision tasks (300-399)
        ImageRecognition = 300,      // Image recognition
        OCR = 303,                   // Optical character recognition
        ImageGeneration = 304,       // Image generation
        ImageEditing = 305,          // Image editing
        ImageClassification = 306,   // Image classification

        // Embedding and retrieval tasks (400-499)
        TextEmbedding = 400,         // Text vectorization
        VectorSearch = 402,          // Vector search
        SemanticSearch = 403         // Semantic search
    };
    
    // Service type group for management convenience
    enum class ServiceGroup {
        NLP,        // Natural Language Processing
        Speech,     // Speech processing
        Vision,     // Computer vision
        Embedding   // Embedding and retrieval
    };
    
    // Convert service type to string representation
    QString serviceTypeToString(ServiceType type);
    
    // Convert string to service type
    ServiceType stringToServiceType(const QString& str);
    
    // Get service group for a service type
    ServiceGroup getServiceGroup(ServiceType type);
    
    // Get string representation of service group
    QString serviceGroupToString(ServiceGroup group);
    
    // Check if service type is valid
    bool isValidServiceType(int type);
    
    // Get all service types in a group
    QList<ServiceType> getServiceTypesInGroup(ServiceGroup group);
}

AIDAEMON_END_NAMESPACE

#endif // AITYPES_H 
