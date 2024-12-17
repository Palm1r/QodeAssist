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
    MessageBuilder &addSystemMessage(const QString &content);

    MessageBuilder &addUserMessage(const QString &content);

    MessageBuilder &addSuffix(const QString &content);

    MessageBuilder &addtTokenizer(PromptTemplate *promptTemplate);

    QString roleToString(MessageRole role) const;

    void saveTo(QJsonObject &request, ProvidersApi api);

private:
    QString m_systemMessage;
    QString m_suffix;
    QVector<Message> m_messages;
    PromptTemplate *m_promptTemplate;
};
} // namespace QodeAssist::LLMCore
