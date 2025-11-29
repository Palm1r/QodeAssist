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
#include <QString>

namespace QodeAssist::LLMCore {

struct InputParameters
{
    std::optional<double> temperature;
    std::optional<double> topP;
    std::optional<int> topK;
    std::optional<int> maxTokens;
    std::optional<double> frequencyPenalty;
    std::optional<double> presencePenalty;

    std::optional<int> thinkingMaxTokens;
    std::optional<int> thinkingBudgetTokens;

    bool stream = true;
    bool enableThinking = false;
    bool enableTools = false;

    virtual ~InputParameters() = default;

    bool operator==(const InputParameters &) const = default;
};

struct OllamaInputParameters : InputParameters
{
    std::optional<QString> keepAlive;

    bool operator==(const OllamaInputParameters &) const = default;
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

    InputParametersBuilder &setThinkingMaxTokens(int tokens) noexcept
    {
        m_params.thinkingMaxTokens = tokens;
        return *this;
    }

    InputParametersBuilder &setThinkingBudgetTokens(int tokens) noexcept
    {
        m_params.thinkingBudgetTokens = tokens;
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
    OllamaInputParametersBuilder &setThinkingMaxTokens(int tokens) noexcept { m_params.thinkingMaxTokens = tokens; return *this; }
    OllamaInputParametersBuilder &setThinkingBudgetTokens(int tokens) noexcept { m_params.thinkingBudgetTokens = tokens; return *this; }
    OllamaInputParametersBuilder &setStream(bool stream) noexcept { m_params.stream = stream; return *this; }
    OllamaInputParametersBuilder &setEnableThinking(bool enable) noexcept { m_params.enableThinking = enable; return *this; }
    OllamaInputParametersBuilder &setEnableTools(bool enable) noexcept { m_params.enableTools = enable; return *this; }

    OllamaInputParametersBuilder &setKeepAlive(QString keepAlive)
    {
        m_params.keepAlive = std::move(keepAlive);
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
    OpenAIResponsesInputParametersBuilder &setThinkingMaxTokens(int tokens) noexcept { m_params.thinkingMaxTokens = tokens; return *this; }
    OpenAIResponsesInputParametersBuilder &setThinkingBudgetTokens(int tokens) noexcept { m_params.thinkingBudgetTokens = tokens; return *this; }
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

