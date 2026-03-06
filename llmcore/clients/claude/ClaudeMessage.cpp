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

#include "ClaudeMessage.hpp"

#include <QJsonArray>
#include <QJsonDocument>

#include "llmcore/core/Log.hpp"

namespace QodeAssist::LLMCore {

ClaudeMessage::ClaudeMessage(QObject *parent)
    : QObject(parent)
{}

void ClaudeMessage::handleContentBlockStart(
    int index, const QString &blockType, const QJsonObject &data)
{
    qCDebug(llmClaudeLog).noquote() << QString("handleContentBlockStart index=%1, blockType=%2")
                    .arg(index)
                    .arg(blockType);

    if (blockType == "text") {
        addCurrentContent<TextContent>();

    } else if (blockType == "image") {
        QJsonObject source = data["source"].toObject();
        QString sourceType = source["type"].toString();
        QString imageData;
        QString mediaType;
        ImageContent::ImageSourceType imgSourceType = ImageContent::ImageSourceType::Base64;

        if (sourceType == "base64") {
            imageData = source["data"].toString();
            mediaType = source["media_type"].toString();
            imgSourceType = ImageContent::ImageSourceType::Base64;
        } else if (sourceType == "url") {
            imageData = source["url"].toString();
            imgSourceType = ImageContent::ImageSourceType::Url;
        }

        addCurrentContent<ImageContent>(imageData, mediaType, imgSourceType);

    } else if (blockType == "tool_use") {
        QString toolId = data["id"].toString();
        QString toolName = data["name"].toString();
        QJsonObject toolInput = data["input"].toObject();

        addCurrentContent<ToolUseContent>(toolId, toolName, toolInput);
        m_pendingToolInputs[index] = "";

    } else if (blockType == "thinking") {
        QString thinking = data["thinking"].toString();
        QString signature = data["signature"].toString();
        qCDebug(llmClaudeLog).noquote() << QString("Creating thinking block with signature length=%1")
                        .arg(signature.length());
        addCurrentContent<ThinkingContent>(thinking, signature);

    } else if (blockType == "redacted_thinking") {
        QString signature = data["signature"].toString();
        qCDebug(llmClaudeLog).noquote() << QString("Creating redacted_thinking block with signature length=%1")
                        .arg(signature.length());
        addCurrentContent<RedactedThinkingContent>(signature);
    }
}

void ClaudeMessage::handleContentBlockDelta(
    int index, const QString &deltaType, const QJsonObject &delta)
{
    if (index >= m_currentBlocks.size()) {
        return;
    }

    if (deltaType == "text_delta") {
        if (auto textContent = qobject_cast<TextContent *>(m_currentBlocks[index])) {
            textContent->appendText(delta["text"].toString());
        }

    } else if (deltaType == "input_json_delta") {
        QString partialJson = delta["partial_json"].toString();
        if (m_pendingToolInputs.contains(index)) {
            m_pendingToolInputs[index] += partialJson;
        }

    } else if (deltaType == "thinking_delta") {
        if (auto thinkingContent = qobject_cast<ThinkingContent *>(m_currentBlocks[index])) {
            thinkingContent->appendThinking(delta["thinking"].toString());
        }
        
    } else if (deltaType == "signature_delta") {
        if (auto thinkingContent = qobject_cast<ThinkingContent *>(m_currentBlocks[index])) {
            QString signature = delta["signature"].toString();
            thinkingContent->setSignature(signature);
            qCDebug(llmClaudeLog).noquote() << QString("Set signature for thinking block %1: length=%2")
                            .arg(index).arg(signature.length());
        } else if (auto redactedContent = qobject_cast<RedactedThinkingContent *>(m_currentBlocks[index])) {
            QString signature = delta["signature"].toString();
            redactedContent->setSignature(signature);
            qCDebug(llmClaudeLog).noquote() << QString("Set signature for redacted_thinking block %1: length=%2")
                            .arg(index).arg(signature.length());
        }
    }
}

void ClaudeMessage::handleContentBlockStop(int index)
{
    if (m_pendingToolInputs.contains(index)) {
        QString jsonInput = m_pendingToolInputs[index];
        QJsonObject inputObject;

        if (!jsonInput.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(jsonInput.toUtf8());
            if (doc.isObject()) {
                inputObject = doc.object();
            }
        }

        if (index < m_currentBlocks.size()) {
            if (auto toolContent = qobject_cast<ToolUseContent *>(m_currentBlocks[index])) {
                toolContent->setInput(inputObject);
            }
        }

        m_pendingToolInputs.remove(index);
    }
}

void ClaudeMessage::handleStopReason(const QString &stopReason)
{
    m_stopReason = stopReason;
    updateStateFromStopReason();
}

QJsonObject ClaudeMessage::toProviderFormat() const
{
    QJsonObject message;
    message["role"] = "assistant";

    QJsonArray content;
    
    for (auto block : m_currentBlocks) {
        QJsonValue blockJson = block->toJson(ProviderFormat::Claude);
        content.append(blockJson);
    }

    message["content"] = content;
    
    qCDebug(llmClaudeLog).noquote() << QString("toProviderFormat - message with %1 content block(s)")
                    .arg(m_currentBlocks.size());
    
    return message;
}

QJsonArray ClaudeMessage::createToolResultsContent(const QHash<QString, QString> &toolResults) const
{
    QJsonArray results;

    for (auto toolContent : getCurrentToolUseContent()) {
        if (toolResults.contains(toolContent->id())) {
            auto toolResult = std::make_unique<ToolResultContent>(
                toolContent->id(), toolResults[toolContent->id()]);
            results.append(toolResult->toJson(ProviderFormat::Claude));
        }
    }

    return results;
}

QList<ToolUseContent *> ClaudeMessage::getCurrentToolUseContent() const
{
    QList<ToolUseContent *> toolBlocks;
    for (auto block : m_currentBlocks) {
        if (auto toolContent = qobject_cast<ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<ThinkingContent *> ClaudeMessage::getCurrentThinkingContent() const
{
    QList<ThinkingContent *> thinkingBlocks;
    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<ThinkingContent *>(block)) {
            thinkingBlocks.append(thinkingContent);
        }
    }
    return thinkingBlocks;
}

QList<RedactedThinkingContent *> ClaudeMessage::getCurrentRedactedThinkingContent() const
{
    QList<RedactedThinkingContent *> redactedBlocks;
    for (auto block : m_currentBlocks) {
        if (auto redactedContent = qobject_cast<RedactedThinkingContent *>(block)) {
            redactedBlocks.append(redactedContent);
        }
    }
    return redactedBlocks;
}

void ClaudeMessage::startNewContinuation()
{
    qCDebug(llmClaudeLog).noquote() << "Starting new continuation";

    m_currentBlocks.clear();
    m_pendingToolInputs.clear();
    m_stopReason.clear();
    m_state = MessageState::Building;
}

void ClaudeMessage::updateStateFromStopReason()
{
    if (m_stopReason == "tool_use" && !getCurrentToolUseContent().empty()) {
        m_state = MessageState::RequiresToolExecution;
    } else if (m_stopReason == "end_turn") {
        m_state = MessageState::Final;
    } else {
        m_state = MessageState::Complete;
    }
}

} // namespace QodeAssist::LLMCore
