/* 
 * Copyright (C) 2025 Petr Mironychev
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

#include "OpenAIMessage.hpp"

#include "logger/Logger.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::Providers {

OpenAIMessage::OpenAIMessage(QObject *parent)
    : QObject(parent)
{}

void OpenAIMessage::handleContentDelta(const QString &content)
{
    auto textContent = getOrCreateTextContent();
    textContent->appendText(content);
}

void OpenAIMessage::handleToolCallStart(int index, const QString &id, const QString &name)
{
    LOG_MESSAGE(QString("OpenAIMessage: handleToolCallStart index=%1, id=%2, name=%3")
                    .arg(index)
                    .arg(id, name));

    while (m_currentBlocks.size() <= index) {
        m_currentBlocks.append(nullptr);
    }

    auto toolContent = new LLMCore::ToolUseContent(id, name);
    toolContent->setParent(this);
    m_currentBlocks[index] = toolContent;
    m_pendingToolArguments[index] = "";
}

void OpenAIMessage::handleToolCallDelta(int index, const QString &argumentsDelta)
{
    if (m_pendingToolArguments.contains(index)) {
        m_pendingToolArguments[index] += argumentsDelta;
    }
}

void OpenAIMessage::handleToolCallComplete(int index)
{
    if (m_pendingToolArguments.contains(index)) {
        QString jsonArgs = m_pendingToolArguments[index];
        QJsonObject argsObject;

        if (!jsonArgs.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonArgs.toUtf8());
            if (doc.isObject()) {
                argsObject = doc.object();
            }
        }

        if (index < m_currentBlocks.size()) {
            if (auto toolContent = qobject_cast<LLMCore::ToolUseContent *>(m_currentBlocks[index])) {
                toolContent->setInput(argsObject);
            }
        }

        m_pendingToolArguments.remove(index);
    }
}

void OpenAIMessage::handleFinishReason(const QString &finishReason)
{
    m_finishReason = finishReason;
    updateStateFromFinishReason();
}

QJsonObject OpenAIMessage::toProviderFormat() const
{
    QJsonObject message;
    message["role"] = "assistant";

    QString textContent;
    QJsonArray toolCalls;

    for (auto block : m_currentBlocks) {
        if (!block)
            continue;

        if (auto text = qobject_cast<LLMCore::TextContent *>(block)) {
            textContent += text->text();
        } else if (auto tool = qobject_cast<LLMCore::ToolUseContent *>(block)) {
            toolCalls.append(tool->toJson(LLMCore::ProviderFormat::OpenAI));
        }
    }

    if (!textContent.isEmpty()) {
        message["content"] = textContent;
    } else {
        message["content"] = QJsonValue();
    }

    if (!toolCalls.isEmpty()) {
        message["tool_calls"] = toolCalls;
    }

    return message;
}

QJsonArray OpenAIMessage::createToolResultMessages(const QHash<QString, QString> &toolResults) const
{
    QJsonArray messages;

    for (auto toolContent : getCurrentToolUseContent()) {
        if (toolResults.contains(toolContent->id())) {
            auto toolResult = std::make_unique<LLMCore::ToolResultContent>(
                toolContent->id(), toolResults[toolContent->id()]);
            messages.append(toolResult->toJson(LLMCore::ProviderFormat::OpenAI));
        }
    }

    return messages;
}

QList<LLMCore::ToolUseContent *> OpenAIMessage::getCurrentToolUseContent() const
{
    QList<LLMCore::ToolUseContent *> toolBlocks;
    for (auto block : m_currentBlocks) {
        if (auto toolContent = qobject_cast<LLMCore::ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

void OpenAIMessage::startNewContinuation()
{
    LOG_MESSAGE(QString("OpenAIAPIMessage: Starting new continuation"));

    m_currentBlocks.clear();
    m_pendingToolArguments.clear();
    m_finishReason.clear();
    m_state = LLMCore::MessageState::Building;
}

void OpenAIMessage::updateStateFromFinishReason()
{
    if (m_finishReason == "tool_calls" && !getCurrentToolUseContent().empty()) {
        m_state = LLMCore::MessageState::RequiresToolExecution;
    } else if (m_finishReason == "stop") {
        m_state = LLMCore::MessageState::Final;
    } else {
        m_state = LLMCore::MessageState::Complete;
    }
}

LLMCore::TextContent *OpenAIMessage::getOrCreateTextContent()
{
    for (auto block : m_currentBlocks) {
        if (auto textContent = qobject_cast<LLMCore::TextContent *>(block)) {
            return textContent;
        }
    }

    return addCurrentContent<LLMCore::TextContent>();
}

} // namespace QodeAssist::Providers
