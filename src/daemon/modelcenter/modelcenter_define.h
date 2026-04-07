// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELCENTER_DEFINE_H
#define MODELCENTER_DEFINE_H

#include "aidaemon_global.h"
#include "common/aitypes.h"

#include <QString>

#include <functional>

AIDAEMON_BEGIN_NAMESPACE

// Use unified service type from AITypes
using ServiceType = AITypes::ServiceType;

// Legacy compatibility - map old InterfaceType to new ServiceType
enum InterfaceType
{
    Chat = static_cast<int>(ServiceType::Chat),
    FunctionCalling = static_cast<int>(ServiceType::FunctionCalling),
    SpeechToText = static_cast<int>(ServiceType::SpeechToText),
    TextToSpeech = static_cast<int>(ServiceType::TextToSpeech),
    ImageRecognition = static_cast<int>(ServiceType::ImageRecognition),
    OCR = static_cast<int>(ServiceType::OCR),
    TextEmbedding = static_cast<int>(ServiceType::TextEmbedding)
};

// Convert between InterfaceType and ServiceType
inline ServiceType interfaceTypeToServiceType(InterfaceType type) {
    return static_cast<ServiceType>(type);
}

inline InterfaceType serviceTypeToInterfaceType(ServiceType type) {
    return static_cast<InterfaceType>(type);
}

struct ChatHistory
{
    QString role;
    QString content;
};

inline constexpr char kSessionChat[] { "chat" };
inline constexpr char kSessionFunctionCalling[] { "function_calling" };
inline constexpr char kSessionSpeechToText[] { "speech_to_text" };
inline constexpr char kSessionTextToSpeech[] { "text_to_speech" };
inline constexpr char kSessionImageRecognition[] { "image_recognition" };

inline constexpr char kChatParamsStream[] { "stream" };
inline constexpr char kChatParamsMessages[] { "messages" };

using ChatStreamer = std::function<bool(const QString &, void *)>;

// ModelCenter result codes
inline constexpr int kModelCenterSuccess = 0;          // Operation successful
inline constexpr int kModelCenterErrorGeneral = -1;    // General error
inline constexpr int kModelCenterErrorRateLimit = 429; // Rate limit exceeded
inline constexpr int kModelCenterErrorServer = 500;    // Server error

// ModelInfo
inline constexpr char kModelInfoModel[] {"Model"};
inline constexpr char kModelInfoApiUrl[] {"api_url"};
inline constexpr char kModelInfoApiKey[] {"api_key"};
inline constexpr char kModelInfoAppID[] {"app_id"};
inline constexpr char kModelInfoApiSecret[] {"api_secret"};
inline constexpr char kModelInfoMaxTokens[] {"MaxTokens"};

AIDAEMON_END_NAMESPACE

#endif // MODELCENTER_DEFINE_H
