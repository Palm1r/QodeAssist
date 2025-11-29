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

#include "OpenAIResponses/ModelRequest.hpp"

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

namespace QodeAssist::OpenAIResponses {

class RequestBuilder
{
public:
    RequestBuilder() = default;

    RequestBuilder &setModel(QString model)
    {
        m_model = std::move(model);
        return *this;
    }

    RequestBuilder &addMessage(Role role, QString content)
    {
        Message msg;
        msg.role = role;
        msg.content.append(MessageContent(std::move(content)));
        m_messages.append(std::move(msg));
        return *this;
    }

    RequestBuilder &addMessage(Message msg)
    {
        m_messages.append(std::move(msg));
        return *this;
    }

    RequestBuilder &setInstructions(QString instructions)
    {
        m_instructions = std::move(instructions);
        return *this;
    }

    RequestBuilder &addTool(Tool tool)
    {
        m_tools.append(std::move(tool));
        return *this;
    }

    RequestBuilder &setTemperature(double temp) noexcept
    {
        m_temperature = temp;
        return *this;
    }

    RequestBuilder &setTopP(double topP) noexcept
    {
        m_topP = topP;
        return *this;
    }

    RequestBuilder &setMaxOutputTokens(int tokens) noexcept
    {
        m_maxOutputTokens = tokens;
        return *this;
    }

    RequestBuilder &setStream(bool stream) noexcept
    {
        m_stream = stream;
        return *this;
    }

    RequestBuilder &setStore(bool store) noexcept
    {
        m_store = store;
        return *this;
    }

    RequestBuilder &setTextFormat(TextFormatOptions format)
    {
        m_textFormat = std::move(format);
        return *this;
    }

    RequestBuilder &setReasoningEffort(ReasoningEffort effort) noexcept
    {
        m_reasoningEffort = effort;
        return *this;
    }

    RequestBuilder &setMetadata(QMap<QString, QVariant> metadata)
    {
        m_metadata = std::move(metadata);
        return *this;
    }

    RequestBuilder &setIncludeReasoningContent(bool include) noexcept
    {
        m_includeReasoningContent = include;
        return *this;
    }
    
    RequestBuilder &clear() noexcept
    {
        m_model.clear();
        m_messages.clear();
        m_instructions.reset();
        m_tools.clear();
        m_temperature.reset();
        m_topP.reset();
        m_maxOutputTokens.reset();
        m_stream = false;
        m_store.reset();
        m_textFormat.reset();
        m_reasoningEffort.reset();
        m_includeReasoningContent = false;
        m_metadata.clear();
        return *this;
    }

    QJsonObject toJson() const
    {
        QJsonObject obj;

        if (!m_model.isEmpty()) {
            obj["model"] = m_model;
        }

        if (!m_messages.isEmpty()) {
            if (m_messages.size() == 1 && m_messages[0].role == Role::User
                && m_messages[0].content.size() == 1) {
                obj["input"] = m_messages[0].content[0].toJson();
            } else {
                QJsonArray input;
                for (const auto &msg : m_messages) {
                    input.append(msg.toJson());
                }
                obj["input"] = input;
            }
        }

        if (m_instructions) {
            obj["instructions"] = *m_instructions;
        }

        if (!m_tools.isEmpty()) {
            QJsonArray tools;
            for (const auto &tool : m_tools) {
                tools.append(tool.toJson());
            }
            obj["tools"] = tools;
        }

        if (m_temperature) {
            obj["temperature"] = *m_temperature;
        }

        if (m_topP) {
            obj["top_p"] = *m_topP;
        }

        if (m_maxOutputTokens) {
            obj["max_output_tokens"] = *m_maxOutputTokens;
        }

        obj["stream"] = m_stream;

        if (m_store) {
            obj["store"] = *m_store;
        }

        if (m_textFormat) {
            QJsonObject textObj;
            textObj["format"] = m_textFormat->toJson();
            obj["text"] = textObj;
        }

        if (m_reasoningEffort) {
            QJsonObject reasoning;
            reasoning["effort"] = effortToString(*m_reasoningEffort);
            obj["reasoning"] = reasoning;
        }

        if (m_includeReasoningContent) {
            QJsonArray include;
            include.append("reasoning.encrypted_content");
            obj["include"] = include;
        }

        if (!m_metadata.isEmpty()) {
            QJsonObject metadata;
            for (auto it = m_metadata.constBegin(); it != m_metadata.constEnd(); ++it) {
                metadata[it.key()] = QJsonValue::fromVariant(it.value());
            }
            obj["metadata"] = metadata;
        }

        return obj;
    }

private:
    QString m_model;
    QList<Message> m_messages;
    std::optional<QString> m_instructions;
    QList<Tool> m_tools;
    std::optional<double> m_temperature;
    std::optional<double> m_topP;
    std::optional<int> m_maxOutputTokens;
    bool m_stream = false;
    std::optional<bool> m_store;
    std::optional<TextFormatOptions> m_textFormat;
    std::optional<ReasoningEffort> m_reasoningEffort;
    bool m_includeReasoningContent = false;
    QMap<QString, QVariant> m_metadata;

    static QString effortToString(ReasoningEffort e)
    {
        switch (e) {
        case ReasoningEffort::None:
            return "none";
        case ReasoningEffort::Minimal:
            return "minimal";
        case ReasoningEffort::Low:
            return "low";
        case ReasoningEffort::Medium:
            return "medium";
        case ReasoningEffort::High:
            return "high";
        }
        return "medium";
    }
};

} // namespace QodeAssist::OpenAIResponses

