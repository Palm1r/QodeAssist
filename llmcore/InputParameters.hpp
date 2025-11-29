/*
 * Copyright (C) 2024-2025 Petr Mironychev
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

#include <optional>
#include <QMap>
#include <QString>
#include <QVariant>

namespace QodeAssist::LLMCore {

struct InputParameters
{
    std::optional<double> temperature;
    std::optional<double> topP;
    std::optional<int> topK;
    std::optional<int> maxTokens;
    std::optional<double> frequencyPenalty;
    std::optional<double> presencePenalty;

    bool stream = true;
    bool enableThinking = false;
    bool enableTools = false;

    virtual ~InputParameters() = default;

    bool operator==(const InputParameters &) const = default;
};

struct ClaudeInputParameters : InputParameters
{
    std::optional<int> thinkingMaxTokens;
    std::optional<int> thinkingBudgetTokens;

    bool operator==(const ClaudeInputParameters &) const = default;
};

struct OllamaInputParameters : InputParameters
{
    std::optional<QString> keepAlive;
    std::optional<bool> think;          // для обычных моделей: true
    std::optional<QString> thinkLevel;  // для GPT-OSS: "low"/"medium"/"high"

    bool operator==(const OllamaInputParameters &) const = default;
};

struct GoogleAIInputParameters : InputParameters
{
    std::optional<int> thinkingMaxTokens;
    std::optional<int> thinkingBudget;
    std::optional<QString> thinkingLevel;
    
    bool operator==(const GoogleAIInputParameters &) const = default;
};

struct OpenAIResponsesInputParameters : InputParameters
{
    std::optional<QString> thinkingEffort;
    std::optional<bool> store;
    std::optional<QMap<QString, QVariant>> metadata;
    bool includeReasoningContent = false;
    
    bool operator==(const OpenAIResponsesInputParameters &) const = default;
};

class InputParametersBuilder
{
public:
    InputParametersBuilder() = default;

    InputParametersBuilder &setTemperature(double temp) noexcept
    {
        m_params.temperature = temp;
        return *this;
    }

    InputParametersBuilder &setTopP(double topP) noexcept
    {
        m_params.topP = topP;
        return *this;
    }

    InputParametersBuilder &setTopK(int topK) noexcept
    {
        m_params.topK = topK;
        return *this;
    }

    InputParametersBuilder &setMaxTokens(int tokens) noexcept
    {
        m_params.maxTokens = tokens;
        return *this;
    }

    InputParametersBuilder &setFrequencyPenalty(double penalty) noexcept
    {
        m_params.frequencyPenalty = penalty;
        return *this;
    }

    InputParametersBuilder &setPresencePenalty(double penalty) noexcept
    {
        m_params.presencePenalty = penalty;
        return *this;
    }

    InputParametersBuilder &setStream(bool stream) noexcept
    {
        m_params.stream = stream;
        return *this;
    }

    InputParametersBuilder &setEnableThinking(bool enable) noexcept
    {
        m_params.enableThinking = enable;
        return *this;
    }

    InputParametersBuilder &setEnableTools(bool enable) noexcept
    {
        m_params.enableTools = enable;
        return *this;
    }

    InputParametersBuilder &clear() noexcept
    {
        m_params = InputParameters{};
        return *this;
    }

    InputParameters build() const noexcept { return m_params; }

private:
    InputParameters m_params;
};

class ClaudeInputParametersBuilder
{
public:
    ClaudeInputParametersBuilder() = default;
    
    explicit ClaudeInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters&>(m_params) = baseParams;
    }

    ClaudeInputParametersBuilder &setTemperature(double temp) noexcept { m_params.temperature = temp; return *this; }
    ClaudeInputParametersBuilder &setTopP(double topP) noexcept { m_params.topP = topP; return *this; }
    ClaudeInputParametersBuilder &setTopK(int topK) noexcept { m_params.topK = topK; return *this; }
    ClaudeInputParametersBuilder &setMaxTokens(int tokens) noexcept { m_params.maxTokens = tokens; return *this; }
    ClaudeInputParametersBuilder &setFrequencyPenalty(double penalty) noexcept { m_params.frequencyPenalty = penalty; return *this; }
    ClaudeInputParametersBuilder &setPresencePenalty(double penalty) noexcept { m_params.presencePenalty = penalty; return *this; }
    ClaudeInputParametersBuilder &setStream(bool stream) noexcept { m_params.stream = stream; return *this; }
    ClaudeInputParametersBuilder &setEnableThinking(bool enable) noexcept { m_params.enableThinking = enable; return *this; }
    ClaudeInputParametersBuilder &setEnableTools(bool enable) noexcept { m_params.enableTools = enable; return *this; }

    ClaudeInputParametersBuilder &setThinkingMaxTokens(int tokens) noexcept
    {
        m_params.thinkingMaxTokens = tokens;
        return *this;
    }

    ClaudeInputParametersBuilder &setThinkingBudgetTokens(int tokens) noexcept
    {
        m_params.thinkingBudgetTokens = tokens;
        return *this;
    }

    ClaudeInputParametersBuilder &clear() noexcept
    {
        m_params = ClaudeInputParameters{};
        return *this;
    }

    ClaudeInputParameters build() const noexcept { return m_params; }

private:
    ClaudeInputParameters m_params;
};

class OllamaInputParametersBuilder
{
public:
    OllamaInputParametersBuilder() = default;

    explicit OllamaInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters&>(m_params) = baseParams;
    }

    OllamaInputParametersBuilder &setTemperature(double temp) noexcept { m_params.temperature = temp; return *this; }
    OllamaInputParametersBuilder &setTopP(double topP) noexcept { m_params.topP = topP; return *this; }
    OllamaInputParametersBuilder &setTopK(int topK) noexcept { m_params.topK = topK; return *this; }
    OllamaInputParametersBuilder &setMaxTokens(int tokens) noexcept { m_params.maxTokens = tokens; return *this; }
    OllamaInputParametersBuilder &setFrequencyPenalty(double penalty) noexcept { m_params.frequencyPenalty = penalty; return *this; }
    OllamaInputParametersBuilder &setPresencePenalty(double penalty) noexcept { m_params.presencePenalty = penalty; return *this; }
    OllamaInputParametersBuilder &setStream(bool stream) noexcept { m_params.stream = stream; return *this; }
    OllamaInputParametersBuilder &setEnableThinking(bool enable) noexcept { m_params.enableThinking = enable; return *this; }
    OllamaInputParametersBuilder &setEnableTools(bool enable) noexcept { m_params.enableTools = enable; return *this; }

    OllamaInputParametersBuilder &setKeepAlive(QString keepAlive)
    {
        m_params.keepAlive = std::move(keepAlive);
        return *this;
    }

    OllamaInputParametersBuilder &setThink(bool think) noexcept
    {
        m_params.think = think;
        return *this;
    }

    OllamaInputParametersBuilder &setThinkLevel(QString level)
    {
        m_params.thinkLevel = std::move(level);
        return *this;
    }

    OllamaInputParametersBuilder &clear() noexcept
    {
        m_params = OllamaInputParameters{};
        return *this;
    }

    OllamaInputParameters build() const noexcept { return m_params; }

private:
    OllamaInputParameters m_params;
};

class GoogleAIInputParametersBuilder
{
public:
    GoogleAIInputParametersBuilder() = default;
    
    explicit GoogleAIInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters&>(m_params) = baseParams;
    }

    GoogleAIInputParametersBuilder &setTemperature(double temp) noexcept { m_params.temperature = temp; return *this; }
    GoogleAIInputParametersBuilder &setTopP(double topP) noexcept { m_params.topP = topP; return *this; }
    GoogleAIInputParametersBuilder &setTopK(int topK) noexcept { m_params.topK = topK; return *this; }
    GoogleAIInputParametersBuilder &setMaxTokens(int tokens) noexcept { m_params.maxTokens = tokens; return *this; }
    GoogleAIInputParametersBuilder &setFrequencyPenalty(double penalty) noexcept { m_params.frequencyPenalty = penalty; return *this; }
    GoogleAIInputParametersBuilder &setPresencePenalty(double penalty) noexcept { m_params.presencePenalty = penalty; return *this; }
    GoogleAIInputParametersBuilder &setStream(bool stream) noexcept { m_params.stream = stream; return *this; }
    
    GoogleAIInputParametersBuilder &setThinkingMaxTokens(int tokens) noexcept
    {
        m_params.thinkingMaxTokens = tokens;
        return *this;
    }
    GoogleAIInputParametersBuilder &setEnableThinking(bool enable) noexcept { m_params.enableThinking = enable; return *this; }
    GoogleAIInputParametersBuilder &setEnableTools(bool enable) noexcept { m_params.enableTools = enable; return *this; }

    GoogleAIInputParametersBuilder &setThinkingBudget(int budget) noexcept
    {
        m_params.thinkingBudget = budget;
        return *this;
    }

    GoogleAIInputParametersBuilder &setThinkingLevel(QString level)
    {
        m_params.thinkingLevel = std::move(level);
        return *this;
    }

    GoogleAIInputParametersBuilder &clear() noexcept
    {
        m_params = GoogleAIInputParameters{};
        return *this;
    }

    GoogleAIInputParameters build() const noexcept { return m_params; }

private:
    GoogleAIInputParameters m_params;
};

class OpenAIResponsesInputParametersBuilder
{
public:
    OpenAIResponsesInputParametersBuilder() = default;
    
    explicit OpenAIResponsesInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters&>(m_params) = baseParams;
    }

    OpenAIResponsesInputParametersBuilder &setTemperature(double temp) noexcept { m_params.temperature = temp; return *this; }
    OpenAIResponsesInputParametersBuilder &setTopP(double topP) noexcept { m_params.topP = topP; return *this; }
    OpenAIResponsesInputParametersBuilder &setTopK(int topK) noexcept { m_params.topK = topK; return *this; }
    OpenAIResponsesInputParametersBuilder &setMaxTokens(int tokens) noexcept { m_params.maxTokens = tokens; return *this; }
    OpenAIResponsesInputParametersBuilder &setFrequencyPenalty(double penalty) noexcept { m_params.frequencyPenalty = penalty; return *this; }
    OpenAIResponsesInputParametersBuilder &setPresencePenalty(double penalty) noexcept { m_params.presencePenalty = penalty; return *this; }
    OpenAIResponsesInputParametersBuilder &setStream(bool stream) noexcept { m_params.stream = stream; return *this; }
    OpenAIResponsesInputParametersBuilder &setEnableThinking(bool enable) noexcept { m_params.enableThinking = enable; return *this; }
    OpenAIResponsesInputParametersBuilder &setEnableTools(bool enable) noexcept { m_params.enableTools = enable; return *this; }

    OpenAIResponsesInputParametersBuilder &setThinkingEffort(QString effort)
    {
        m_params.thinkingEffort = std::move(effort);
        return *this;
    }

    OpenAIResponsesInputParametersBuilder &setStore(bool store) noexcept
    {
        m_params.store = store;
        return *this;
    }

    OpenAIResponsesInputParametersBuilder &setMetadata(QMap<QString, QVariant> metadata)
    {
        m_params.metadata = std::move(metadata);
        return *this;
    }

    OpenAIResponsesInputParametersBuilder &setIncludeReasoningContent(bool include) noexcept
    {
        m_params.includeReasoningContent = include;
        return *this;
    }

    OpenAIResponsesInputParametersBuilder &clear() noexcept
    {
        m_params = OpenAIResponsesInputParameters{};
        return *this;
    }

    OpenAIResponsesInputParameters build() const noexcept { return m_params; }

private:
    OpenAIResponsesInputParameters m_params;
};

} // namespace QodeAssist::LLMCore

