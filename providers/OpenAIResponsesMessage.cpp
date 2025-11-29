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
#include "OpenAIResponses/ResponseObject.hpp"

#include "logger/Logger.hpp"

#include <QJsonArray>

namespace QodeAssist::Providers {

OpenAIResponsesMessage::OpenAIResponsesMessage(QObject *parent)
    : QObject(parent)
{}

void OpenAIResponsesMessage::handleItemDelta(const QJsonObject &item)
{
    using namespace QodeAssist::OpenAIResponses;

    const QString itemType = item["type"].toString();

    if (itemType == "message" || (itemType.isEmpty() && item.contains("content"))) {
        OutputItem outputItem = OutputItem::fromJson(item);

        if (const auto *msg = outputItem.asMessage()) {
            for (const auto &outputText : msg->outputTexts) {
                if (!outputText.text.isEmpty()) {
                    auto textItem = getOrCreateTextItem();
                    textItem->appendText(outputText.text);
                }
            }
        }
    }
}

void OpenAIResponsesMessage::handleToolCallStart(const QString &callId, const QString &name)
{
    auto toolContent = new LLMCore::ToolUseContent(callId, name);
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
    auto thinkingContent = new LLMCore::ThinkingContent();
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
    QList<LLMCore::ToolUseContent *> toolCalls;

    for (const auto *block : m_items) {
        if (const auto *text = qobject_cast<const LLMCore::TextContent *>(block)) {
            textContent += text->text();
        } else if (auto *tool = qobject_cast<LLMCore::ToolUseContent *>(
                       const_cast<LLMCore::ContentBlock *>(block))) {
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

QList<LLMCore::ToolUseContent *> OpenAIResponsesMessage::getCurrentToolUseContent() const
{
    QList<LLMCore::ToolUseContent *> toolBlocks;
    for (auto *block : m_items) {
        if (auto *toolContent = qobject_cast<LLMCore::ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<LLMCore::ThinkingContent *> OpenAIResponsesMessage::getCurrentThinkingContent() const
{
    QList<LLMCore::ThinkingContent *> thinkingBlocks;
    for (auto *block : m_items) {
        if (auto *thinkingContent = qobject_cast<LLMCore::ThinkingContent *>(block)) {
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
        if (const auto *textContent = qobject_cast<const LLMCore::TextContent *>(block)) {
            text += textContent->text();
        }
    }
    return text;
}

void OpenAIResponsesMessage::updateStateFromStatus()
{
    using namespace QodeAssist::OpenAIResponses;

    if (m_status == "completed") {
        if (!getCurrentToolUseContent().isEmpty()) {
            m_state = LLMCore::MessageState::RequiresToolExecution;
        } else {
            m_state = LLMCore::MessageState::Complete;
        }
    } else if (m_status == "in_progress") {
        m_state = LLMCore::MessageState::Building;
    } else if (m_status == "failed" || m_status == "cancelled" || m_status == "incomplete") {
        m_state = LLMCore::MessageState::Final;
    } else {
        m_state = LLMCore::MessageState::Building;
    }
}

LLMCore::TextContent *OpenAIResponsesMessage::getOrCreateTextItem()
{
    for (auto *block : m_items) {
        if (auto *textContent = qobject_cast<LLMCore::TextContent *>(block)) {
            return textContent;
        }
    }

    auto *textContent = new LLMCore::TextContent();
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
    m_state = LLMCore::MessageState::Building;
}

} // namespace QodeAssist::Providers

