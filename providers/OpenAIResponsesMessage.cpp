// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

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
    auto toolContent = new PluginLLMCore::ToolUseContent(callId, name);
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
    auto thinkingContent = new PluginLLMCore::ThinkingContent();
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
    QList<PluginLLMCore::ToolUseContent *> toolCalls;

    for (const auto *block : m_items) {
        if (const auto *text = qobject_cast<const PluginLLMCore::TextContent *>(block)) {
            textContent += text->text();
        } else if (auto *tool = qobject_cast<PluginLLMCore::ToolUseContent *>(
                       const_cast<PluginLLMCore::ContentBlock *>(block))) {
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

QList<PluginLLMCore::ToolUseContent *> OpenAIResponsesMessage::getCurrentToolUseContent() const
{
    QList<PluginLLMCore::ToolUseContent *> toolBlocks;
    for (auto *block : m_items) {
        if (auto *toolContent = qobject_cast<PluginLLMCore::ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<PluginLLMCore::ThinkingContent *> OpenAIResponsesMessage::getCurrentThinkingContent() const
{
    QList<PluginLLMCore::ThinkingContent *> thinkingBlocks;
    for (auto *block : m_items) {
        if (auto *thinkingContent = qobject_cast<PluginLLMCore::ThinkingContent *>(block)) {
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
        if (const auto *textContent = qobject_cast<const PluginLLMCore::TextContent *>(block)) {
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
            m_state = PluginLLMCore::MessageState::RequiresToolExecution;
        } else {
            m_state = PluginLLMCore::MessageState::Complete;
        }
    } else if (m_status == "in_progress") {
        m_state = PluginLLMCore::MessageState::Building;
    } else if (m_status == "failed" || m_status == "cancelled" || m_status == "incomplete") {
        m_state = PluginLLMCore::MessageState::Final;
    } else {
        m_state = PluginLLMCore::MessageState::Building;
    }
}

PluginLLMCore::TextContent *OpenAIResponsesMessage::getOrCreateTextItem()
{
    for (auto *block : m_items) {
        if (auto *textContent = qobject_cast<PluginLLMCore::TextContent *>(block)) {
            return textContent;
        }
    }

    auto *textContent = new PluginLLMCore::TextContent();
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
    m_state = PluginLLMCore::MessageState::Building;
}

} // namespace QodeAssist::Providers

