/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "PromptTemplate.hpp"

namespace QodeAssist::LLMCore {

enum class MessageRole { System, User, Assistant };

enum class OllamaFormat { Messages, Completions };

enum class ProvidersApi { Ollama, OpenAI };

static const QString ROLE_SYSTEM = "system";
static const QString ROLE_USER = "user";
static const QString ROLE_ASSISTANT = "assistant";

struct Message
{
    MessageRole role;
    QString content;
};

class MessageBuilder
{
public:
    MessageBuilder &addSystemMessage(const QString &content)
    {
        m_systemMessage = content;
        return *this;
    }

    MessageBuilder &addUserMessage(const QString &content)
    {
        m_messages.append({MessageRole::User, content});
        return *this;
    }

    MessageBuilder &addSuffix(const QString &content)
    {
        m_suffix = content;
        return *this;
    }

    MessageBuilder &addtTokenizer(PromptTemplate *promptTemplate)
    {
        m_promptTemplate = promptTemplate;
        return *this;
    }

    QString roleToString(MessageRole role) const
    {
        switch (role) {
        case MessageRole::System:
            return ROLE_SYSTEM;
        case MessageRole::User:
            return ROLE_USER;
        case MessageRole::Assistant:
            return ROLE_ASSISTANT;
        default:
            return ROLE_USER;
        }
    }

    void saveTo(QJsonObject &request, ProvidersApi api)
    {
        if (!m_promptTemplate) {
            return;
        }

        if (api == ProvidersApi::Ollama) {
            ContextData context{
                m_messages.isEmpty() ? QString() : m_messages.last().content,
                m_suffix,
                m_systemMessage};

            if (m_promptTemplate->type() == TemplateType::Fim) {
                m_promptTemplate->prepareRequest(request, context);
            } else {
                QJsonArray messages;

                messages.append(QJsonObject{{"role", "system"}, {"content", m_systemMessage}});
                messages.append(
                    QJsonObject{{"role", "user"}, {"content", m_messages.last().content}});
                request["messages"] = messages;
                m_promptTemplate->prepareRequest(request, {});
            }
        } else if (api == ProvidersApi::OpenAI) {
            QJsonArray messages;

            messages.append(QJsonObject{{"role", "system"}, {"content", m_systemMessage}});
            messages.append(QJsonObject{{"role", "user"}, {"content", m_messages.last().content}});
            request["messages"] = messages;
            m_promptTemplate->prepareRequest(request, {});
        }
    }

private:
    QString m_systemMessage;
    QString m_suffix;
    QVector<Message> m_messages;
    PromptTemplate *m_promptTemplate;
};
} // namespace QodeAssist::LLMCore
