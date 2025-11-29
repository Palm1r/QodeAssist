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

#include "ModelRequest.hpp"

#include <QJsonObject>
#include <QString>

namespace QodeAssist::OpenAIResponses {

struct InputTokensRequest
{
    std::optional<QString> conversation;
    std::optional<QJsonArray> input;
    std::optional<QString> instructions;
    std::optional<QString> model;
    std::optional<bool> parallelToolCalls;
    std::optional<QString> previousResponseId;
    std::optional<QJsonObject> reasoning;
    std::optional<QJsonObject> text;
    std::optional<QJsonValue> toolChoice;
    std::optional<QJsonArray> tools;
    std::optional<QString> truncation;

    QString buildUrl(const QString &baseUrl) const
    {
        return QString("%1/v1/responses/input_tokens").arg(baseUrl);
    }

    QJsonObject toJson() const
    {
        QJsonObject obj;

        if (conversation)
            obj["conversation"] = *conversation;
        if (input)
            obj["input"] = *input;
        if (instructions)
            obj["instructions"] = *instructions;
        if (model)
            obj["model"] = *model;
        if (parallelToolCalls)
            obj["parallel_tool_calls"] = *parallelToolCalls;
        if (previousResponseId)
            obj["previous_response_id"] = *previousResponseId;
        if (reasoning)
            obj["reasoning"] = *reasoning;
        if (text)
            obj["text"] = *text;
        if (toolChoice)
            obj["tool_choice"] = *toolChoice;
        if (tools)
            obj["tools"] = *tools;
        if (truncation)
            obj["truncation"] = *truncation;

        return obj;
    }

    bool isValid() const { return input.has_value() || previousResponseId.has_value(); }
};

class InputTokensRequestBuilder
{
public:
    InputTokensRequestBuilder &setConversation(const QString &conversationId)
    {
        m_request.conversation = conversationId;
        return *this;
    }

    InputTokensRequestBuilder &setInput(const QJsonArray &input)
    {
        m_request.input = input;
        return *this;
    }

    InputTokensRequestBuilder &addInputMessage(const Message &message)
    {
        if (!m_request.input) {
            m_request.input = QJsonArray();
        }
        m_request.input->append(message.toJson());
        return *this;
    }

    InputTokensRequestBuilder &setInstructions(const QString &instructions)
    {
        m_request.instructions = instructions;
        return *this;
    }

    InputTokensRequestBuilder &setModel(const QString &model)
    {
        m_request.model = model;
        return *this;
    }

    InputTokensRequestBuilder &setParallelToolCalls(bool enabled)
    {
        m_request.parallelToolCalls = enabled;
        return *this;
    }

    InputTokensRequestBuilder &setPreviousResponseId(const QString &responseId)
    {
        m_request.previousResponseId = responseId;
        return *this;
    }

    InputTokensRequestBuilder &setReasoning(const QJsonObject &reasoning)
    {
        m_request.reasoning = reasoning;
        return *this;
    }

    InputTokensRequestBuilder &setReasoningEffort(ReasoningEffort effort)
    {
        QString effortStr;
        switch (effort) {
        case ReasoningEffort::None:
            effortStr = "none";
            break;
        case ReasoningEffort::Minimal:
            effortStr = "minimal";
            break;
        case ReasoningEffort::Low:
            effortStr = "low";
            break;
        case ReasoningEffort::Medium:
            effortStr = "medium";
            break;
        case ReasoningEffort::High:
            effortStr = "high";
            break;
        }
        m_request.reasoning = QJsonObject{{"effort", effortStr}};
        return *this;
    }

    InputTokensRequestBuilder &setText(const QJsonObject &text)
    {
        m_request.text = text;
        return *this;
    }

    InputTokensRequestBuilder &setTextFormat(const TextFormatOptions &format)
    {
        m_request.text = format.toJson();
        return *this;
    }

    InputTokensRequestBuilder &setToolChoice(const QJsonValue &toolChoice)
    {
        m_request.toolChoice = toolChoice;
        return *this;
    }

    InputTokensRequestBuilder &setTools(const QJsonArray &tools)
    {
        m_request.tools = tools;
        return *this;
    }

    InputTokensRequestBuilder &addTool(const Tool &tool)
    {
        if (!m_request.tools) {
            m_request.tools = QJsonArray();
        }
        m_request.tools->append(tool.toJson());
        return *this;
    }

    InputTokensRequestBuilder &setTruncation(const QString &truncation)
    {
        m_request.truncation = truncation;
        return *this;
    }

    InputTokensRequest build() const { return m_request; }

private:
    InputTokensRequest m_request;
};

struct InputTokensResponse
{
    QString object;
    int inputTokens = 0;

    static InputTokensResponse fromJson(const QJsonObject &obj)
    {
        InputTokensResponse result;
        result.object = obj["object"].toString();
        result.inputTokens = obj["input_tokens"].toInt();
        return result;
    }
};

} // namespace QodeAssist::OpenAIResponses

