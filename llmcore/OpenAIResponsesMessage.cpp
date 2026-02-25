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

#include "OpenAIResponsesMessage.hpp"

#include <Logger.hpp>

#include <QJsonArray>

namespace QodeAssist::LLMCore {

OpenAIResponsesMessage::OpenAIResponsesMessage(QObject *parent)
    : QObject(parent)
{}

void OpenAIResponsesMessage::handleContentDelta(const QString &text)
{
    if (!text.isEmpty()) {
        auto textItem = getOrCreateTextItem();
        textItem->appendText(text);
    }
}

void OpenAIResponsesMessage::handleToolCallStart(const QString &callId, const QString &name)
{
    auto toolContent = new ToolUseContent(callId, name);
    toolContent->setParent(this);
    m_items.append(toolContent);
    m_toolCalls[callId] = toolContent;
    m_pendingToolArguments[callId] = "";
}

void OpenAIResponsesMessage::handleToolCallDelta(const QString &callId, const QString &argumentsDelta)
{
    if (m_pendingToolArguments.contains(callId)) {
        m_pendingToolArguments[callId] += argumentsDelta;
    }
}

void OpenAIResponsesMessage::handleToolCallComplete(const QString &callId)
{
    if (m_pendingToolArguments.contains(callId) && m_toolCalls.contains(callId)) {
        QString jsonArgs = m_pendingToolArguments[callId];
        QJsonObject argsObject;

        if (!jsonArgs.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonArgs.toUtf8());
            if (doc.isObject()) {
                argsObject = doc.object();
            }
        }

        m_toolCalls[callId]->setInput(argsObject);
        m_pendingToolArguments.remove(callId);
    }
}

void OpenAIResponsesMessage::handleReasoningStart(const QString &itemId)
{
    auto thinkingContent = new ThinkingContent();
    thinkingContent->setParent(this);
    m_items.append(thinkingContent);
    m_thinkingBlocks[itemId] = thinkingContent;
}

void OpenAIResponsesMessage::handleReasoningDelta(const QString &itemId, const QString &text)
{
    if (m_thinkingBlocks.contains(itemId)) {
        m_thinkingBlocks[itemId]->appendThinking(text);
    }
}

void OpenAIResponsesMessage::handleReasoningComplete(const QString &itemId)
{
    Q_UNUSED(itemId);
}

void OpenAIResponsesMessage::handleStatus(const QString &status)
{
    m_status = status;
    updateStateFromStatus();
}

QList<QJsonObject> OpenAIResponsesMessage::toItemsFormat() const
{
    QList<QJsonObject> items;

    QString textContent;
    QList<ToolUseContent *> toolCalls;

    for (const auto *block : m_items) {
        if (const auto *text = qobject_cast<const TextContent *>(block)) {
            textContent += text->text();
        } else if (auto *tool = qobject_cast<ToolUseContent *>(
                       const_cast<ContentBlock *>(block))) {
            toolCalls.append(tool);
        }
    }

    if (!textContent.isEmpty()) {
        QJsonObject message;
        message["role"] = "assistant";
        message["content"] = textContent;
        items.append(message);
    }

    for (const auto *tool : toolCalls) {
        QJsonObject functionCallItem;
        functionCallItem["type"] = "function_call";
        functionCallItem["call_id"] = tool->id();
        functionCallItem["name"] = tool->name();
        functionCallItem["arguments"] = QString::fromUtf8(
            QJsonDocument(tool->input()).toJson(QJsonDocument::Compact));
        items.append(functionCallItem);
    }

    return items;
}

QList<ToolUseContent *> OpenAIResponsesMessage::getCurrentToolUseContent() const
{
    QList<ToolUseContent *> toolBlocks;
    for (auto *block : m_items) {
        if (auto *toolContent = qobject_cast<ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<ThinkingContent *> OpenAIResponsesMessage::getCurrentThinkingContent() const
{
    QList<ThinkingContent *> thinkingBlocks;
    for (auto *block : m_items) {
        if (auto *thinkingContent = qobject_cast<ThinkingContent *>(block)) {
            thinkingBlocks.append(thinkingContent);
        }
    }
    return thinkingBlocks;
}

QJsonArray OpenAIResponsesMessage::createToolResultItems(const QHash<QString, QString> &toolResults) const
{
    QJsonArray items;

    for (const auto *toolContent : getCurrentToolUseContent()) {
        if (toolResults.contains(toolContent->id())) {
            QJsonObject toolResultItem;
            toolResultItem["type"] = "function_call_output";
            toolResultItem["call_id"] = toolContent->id();
            toolResultItem["output"] = toolResults[toolContent->id()];
            items.append(toolResultItem);
        }
    }

    return items;
}

QString OpenAIResponsesMessage::accumulatedText() const
{
    QString text;
    for (const auto *block : m_items) {
        if (const auto *textContent = qobject_cast<const TextContent *>(block)) {
            text += textContent->text();
        }
    }
    return text;
}

void OpenAIResponsesMessage::updateStateFromStatus()
{
    if (m_status == "completed") {
        if (!getCurrentToolUseContent().isEmpty()) {
            m_state = MessageState::RequiresToolExecution;
        } else {
            m_state = MessageState::Complete;
        }
    } else if (m_status == "in_progress") {
        m_state = MessageState::Building;
    } else if (m_status == "failed" || m_status == "cancelled" || m_status == "incomplete") {
        m_state = MessageState::Final;
    } else {
        m_state = MessageState::Building;
    }
}

TextContent *OpenAIResponsesMessage::getOrCreateTextItem()
{
    for (auto *block : m_items) {
        if (auto *textContent = qobject_cast<TextContent *>(block)) {
            return textContent;
        }
    }

    auto *textContent = new TextContent();
    textContent->setParent(this);
    m_items.append(textContent);
    return textContent;
}

void OpenAIResponsesMessage::startNewContinuation()
{
    m_toolCalls.clear();
    m_thinkingBlocks.clear();

    qDeleteAll(m_items);
    m_items.clear();

    m_pendingToolArguments.clear();
    m_status.clear();
    m_state = MessageState::Building;
}

} // namespace QodeAssist::LLMCore
