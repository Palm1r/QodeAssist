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

// CRTP base class for common builder methods
template<typename Derived, typename ParamsType>
class InputParametersBuilderBase
{
protected:
    ParamsType m_params;

    Derived &self() noexcept { return static_cast<Derived &>(*this); }
    const Derived &self() const noexcept { return static_cast<const Derived &>(*this); }

public:
    Derived &setTemperature(double temp) noexcept
    {
        m_params.temperature = temp;
        return self();
    }

    Derived &setTopP(double topP) noexcept
    {
        m_params.topP = topP;
        return self();
    }

    Derived &setTopK(int topK) noexcept
    {
        m_params.topK = topK;
        return self();
    }

    Derived &setMaxTokens(int tokens) noexcept
    {
        m_params.maxTokens = tokens;
        return self();
    }

    Derived &setFrequencyPenalty(double penalty) noexcept
    {
        m_params.frequencyPenalty = penalty;
        return self();
    }

    Derived &setPresencePenalty(double penalty) noexcept
    {
        m_params.presencePenalty = penalty;
        return self();
    }

    Derived &setStream(bool stream) noexcept
    {
        m_params.stream = stream;
        return self();
    }

    Derived &setEnableThinking(bool enable) noexcept
    {
        m_params.enableThinking = enable;
        return self();
    }

    Derived &setEnableTools(bool enable) noexcept
    {
        m_params.enableTools = enable;
        return self();
    }

    Derived &clear() noexcept
    {
        m_params = ParamsType{};
        return self();
    }

    ParamsType build() const noexcept { return m_params; }
};

class InputParametersBuilder
    : public InputParametersBuilderBase<InputParametersBuilder, InputParameters>
{
public:
    InputParametersBuilder() = default;
};

class ClaudeInputParametersBuilder
    : public InputParametersBuilderBase<ClaudeInputParametersBuilder, ClaudeInputParameters>
{
public:
    ClaudeInputParametersBuilder() = default;

    explicit ClaudeInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters &>(m_params) = baseParams;
    }

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
};

class OllamaInputParametersBuilder
    : public InputParametersBuilderBase<OllamaInputParametersBuilder, OllamaInputParameters>
{
public:
    OllamaInputParametersBuilder() = default;

    explicit OllamaInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters &>(m_params) = baseParams;
    }

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
};

class GoogleAIInputParametersBuilder
    : public InputParametersBuilderBase<GoogleAIInputParametersBuilder, GoogleAIInputParameters>
{
public:
    GoogleAIInputParametersBuilder() = default;

    explicit GoogleAIInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters &>(m_params) = baseParams;
    }

    GoogleAIInputParametersBuilder &setThinkingMaxTokens(int tokens) noexcept
    {
        m_params.thinkingMaxTokens = tokens;
        return *this;
    }

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
};

class OpenAIResponsesInputParametersBuilder
    : public InputParametersBuilderBase<OpenAIResponsesInputParametersBuilder,
                                        OpenAIResponsesInputParameters>
{
public:
    OpenAIResponsesInputParametersBuilder() = default;

    explicit OpenAIResponsesInputParametersBuilder(InputParametersBuilder &&base)
    {
        const auto baseParams = base.build();
        static_cast<InputParameters &>(m_params) = baseParams;
    }

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
};

} // namespace QodeAssist::LLMCore

