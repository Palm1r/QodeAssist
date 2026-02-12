/*
 * Copyright (C) 2026 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QJsonObject>

#include <llmcore/ClaudeConfig.hpp>
#include <llmcore/GoogleAIConfig.hpp>
#include <llmcore/LMStudioConfig.hpp>
#include <llmcore/MistralConfig.hpp>
#include <llmcore/OllamaConfig.hpp>
#include <llmcore/OpenAIConfig.hpp>
#include <llmcore/OpenAIResponsesConfig.hpp>
#include <llmcore/ProviderID.hpp>

namespace QodeAssist {

class ConfigBuilder
{
public:
    template<typename Settings>
    static QJsonObject forClaude(const Settings &s)
    {
        auto config = LLMCore::ClaudeConfig()
                          .maxTokens(s.maxTokens())
                          .temperature(s.temperature());

        if (s.useTopP())
            config.topP(s.topP());
        if (s.useTopK())
            config.topK(s.topK());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forOllama(const Settings &s)
    {
        auto config = LLMCore::OllamaConfig()
                          .numPredict(s.maxTokens())
                          .temperature(s.temperature())
                          .keepAlive(s.ollamaLivetime());

        if (s.useTopP())
            config.topP(s.topP());
        if (s.useTopK())
            config.topK(s.topK());
        if (s.useFrequencyPenalty())
            config.frequencyPenalty(s.frequencyPenalty());
        if (s.usePresencePenalty())
            config.presencePenalty(s.presencePenalty());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forOpenAI(const Settings &s)
    {
        auto config = LLMCore::OpenAIConfig()
                          .maxTokens(s.maxTokens())
                          .temperature(s.temperature());

        if (s.useTopP())
            config.topP(s.topP());
        if (s.useTopK())
            config.topK(s.topK());
        if (s.useFrequencyPenalty())
            config.frequencyPenalty(s.frequencyPenalty());
        if (s.usePresencePenalty())
            config.presencePenalty(s.presencePenalty());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forOpenAIResponses(const Settings &s)
    {
        auto config = LLMCore::OpenAIResponsesConfig()
                          .maxOutputTokens(s.maxTokens());

        if (s.useTopP())
            config.topP(s.topP());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forGoogleAI(const Settings &s)
    {
        auto config = LLMCore::GoogleAIConfig()
                          .maxOutputTokens(s.maxTokens())
                          .temperature(s.temperature());

        if (s.useTopP())
            config.topP(s.topP());
        if (s.useTopK())
            config.topK(s.topK());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forLMStudio(const Settings &s)
    {
        auto config = LLMCore::LMStudioConfig()
                          .maxTokens(s.maxTokens())
                          .temperature(s.temperature());

        if (s.useTopP())
            config.topP(s.topP());
        if (s.useTopK())
            config.topK(s.topK());
        if (s.useFrequencyPenalty())
            config.frequencyPenalty(s.frequencyPenalty());
        if (s.usePresencePenalty())
            config.presencePenalty(s.presencePenalty());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forMistral(const Settings &s)
    {
        auto config = LLMCore::MistralConfig()
                          .maxTokens(s.maxTokens())
                          .temperature(s.temperature());

        if (s.useTopP())
            config.topP(s.topP());
        if (s.useTopK())
            config.topK(s.topK());
        if (s.useFrequencyPenalty())
            config.frequencyPenalty(s.frequencyPenalty());
        if (s.usePresencePenalty())
            config.presencePenalty(s.presencePenalty());

        return config.json();
    }

    template<typename Settings>
    static QJsonObject forGeneric(const Settings &s)
    {
        QJsonObject config;
        config["max_tokens"] = s.maxTokens();
        config["temperature"] = s.temperature();
        if (s.useTopP())
            config["top_p"] = s.topP();
        if (s.useTopK())
            config["top_k"] = s.topK();
        if (s.useFrequencyPenalty())
            config["frequency_penalty"] = s.frequencyPenalty();
        if (s.usePresencePenalty())
            config["presence_penalty"] = s.presencePenalty();
        return config;
    }

    template<typename Settings>
    static QJsonObject create(LLMCore::ProviderID id, const Settings &s)
    {
        switch (id) {
        case LLMCore::ProviderID::Claude:
            return forClaude(s);
        case LLMCore::ProviderID::Ollama:
            return forOllama(s);
        case LLMCore::ProviderID::OpenAI:
            return forOpenAI(s);
        case LLMCore::ProviderID::OpenAIResponses:
            return forOpenAIResponses(s);
        case LLMCore::ProviderID::GoogleAI:
            return forGoogleAI(s);
        case LLMCore::ProviderID::LMStudio:
            return forLMStudio(s);
        case LLMCore::ProviderID::MistralAI:
            return forMistral(s);
        case LLMCore::ProviderID::OpenAICompatible:
        case LLMCore::ProviderID::LlamaCpp:
        case LLMCore::ProviderID::OpenRouter:
            return forOpenAI(s);
        default:
            return forGeneric(s);
        }
    }

    template<typename Settings>
    static void addThinking(
        QJsonObject &config, LLMCore::ProviderID id, const Settings &s)
    {
        switch (id) {
        case LLMCore::ProviderID::Claude:
            config["thinking"] = QJsonObject{
                {"type", "enabled"},
                {"budget_tokens", s.thinkingBudgetTokens()},
            };
            break;
        case LLMCore::ProviderID::Ollama:
            config["enable_thinking"] = true;
            break;
        case LLMCore::ProviderID::GoogleAI:
            config["thinking_budget"] = s.thinkingBudgetTokens();
            config["thinking_max_tokens"] = s.thinkingMaxTokens();
            break;
        case LLMCore::ProviderID::OpenAIResponses:
            config["reasoning"] = QJsonObject{
                {"effort", s.openAIResponsesReasoningEffort.stringValue().toLower()},
            };
            config["thinking_max_tokens"] = s.thinkingMaxTokens();
            break;
        default:
            break;
        }
    }
};

} // namespace QodeAssist
