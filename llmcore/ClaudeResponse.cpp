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

#include "ClaudeResponse.hpp"
#include "logger/Logger.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::LLMCore {

ClaudeResponse::ClaudeResponse(QObject *parent)
    : QObject(parent)
{}

StreamEvent ClaudeResponse::processEvent(const QJsonObject &event)
{
    StreamEvent result;
    QString eventType = event["type"].toString();

    if (eventType == "message_stop") {
        return result;
    }

    if (eventType == "message_start") {
        startNewContinuation();
        result.type = StreamEvent::Type::MessageStart;
        LOG_MESSAGE(QString("ClaudeResponse: message_start"));

    } else if (eventType == "content_block_start") {
        int index = event["index"].toInt();
        QJsonObject contentBlock = event["content_block"].toObject();
        QString blockType = contentBlock["type"].toString();
        handleContentBlockStart(index, blockType, contentBlock);

    } else if (eventType == "content_block_delta") {
        int index = event["index"].toInt();
        QJsonObject delta = event["delta"].toObject();
        QString deltaType = delta["type"].toString();

        handleContentBlockDelta(index, deltaType, delta);

        if (deltaType == "text_delta") {
            result.type = StreamEvent::Type::TextDelta;
            result.text = delta["text"].toString();
        }

    } else if (eventType == "content_block_stop") {
        int index = event["index"].toInt();
        result.blockIndex = index;

        if (event.contains("content_block")) {
            QJsonObject contentBlock = event["content_block"].toObject();
            QString blockType = contentBlock["type"].toString();

            if (blockType == "thinking" || blockType == "redacted_thinking") {
                QString signature = contentBlock["signature"].toString();
                if (!signature.isEmpty() && index < m_currentBlocks.size()) {
                    if (auto thinkingContent = qobject_cast<ThinkingContent *>(m_currentBlocks[index])) {
                        thinkingContent->setSignature(signature);
                    } else if (auto redactedContent = qobject_cast<RedactedThinkingContent *>(m_currentBlocks[index])) {
                        redactedContent->setSignature(signature);
                    }
                }
            }
        }

        handleContentBlockStop(index);

        if (index < m_currentBlocks.size()) {
            if (auto thinkingContent = qobject_cast<ThinkingContent *>(m_currentBlocks[index])) {
                result.type = StreamEvent::Type::ThinkingBlockComplete;
                result.thinking = thinkingContent->thinking();
                result.signature = thinkingContent->signature();
            } else if (auto redactedContent = qobject_cast<RedactedThinkingContent *>(m_currentBlocks[index])) {
                result.type = StreamEvent::Type::RedactedThinkingBlockComplete;
                result.signature = redactedContent->signature();
            }
        }

    } else if (eventType == "message_delta") {
        QJsonObject delta = event["delta"].toObject();
        if (delta.contains("stop_reason")) {
            QString stopReason = delta["stop_reason"].toString();
            handleStopReason(stopReason);
            result.type = StreamEvent::Type::MessageComplete;
        }
    }

    return result;
}

void ClaudeResponse::handleContentBlockStart(
    int index, const QString &blockType, const QJsonObject &data)
{
    LOG_MESSAGE(QString("ClaudeResponse: handleContentBlockStart index=%1, blockType=%2")
                    .arg(index)
                    .arg(blockType));

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
        LOG_MESSAGE(QString("ClaudeResponse: Creating thinking block with signature length=%1")
                        .arg(signature.length()));
        addCurrentContent<ThinkingContent>(thinking, signature);

    } else if (blockType == "redacted_thinking") {
        QString signature = data["signature"].toString();
        LOG_MESSAGE(QString("ClaudeResponse: Creating redacted_thinking block with signature length=%1")
                        .arg(signature.length()));
        addCurrentContent<RedactedThinkingContent>(signature);
    }
}

void ClaudeResponse::handleContentBlockDelta(
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
            LOG_MESSAGE(QString("Set signature for thinking block %1: length=%2")
                            .arg(index).arg(signature.length()));
        } else if (auto redactedContent = qobject_cast<RedactedThinkingContent *>(m_currentBlocks[index])) {
            QString signature = delta["signature"].toString();
            redactedContent->setSignature(signature);
            LOG_MESSAGE(QString("Set signature for redacted_thinking block %1: length=%2")
                            .arg(index).arg(signature.length()));
        }
    }
}

void ClaudeResponse::handleContentBlockStop(int index)
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

void ClaudeResponse::handleStopReason(const QString &stopReason)
{
    m_stopReason = stopReason;
    updateStateFromStopReason();
}

QJsonObject ClaudeResponse::toProviderFormat() const
{
    QJsonObject message;
    message["role"] = "assistant";

    QJsonArray content;

    for (auto block : m_currentBlocks) {
        QJsonValue blockJson = block->toJson(ProviderFormat::Claude);
        content.append(blockJson);
    }

    message["content"] = content;

    LOG_MESSAGE(QString("ClaudeResponse::toProviderFormat - message with %1 content block(s)")
                    .arg(m_currentBlocks.size()));

    return message;
}

QJsonArray ClaudeResponse::createToolResultsContent(const QHash<QString, QString> &toolResults) const
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

QList<ToolUseContent *> ClaudeResponse::getCurrentToolUseContent() const
{
    QList<ToolUseContent *> toolBlocks;
    for (auto block : m_currentBlocks) {
        if (auto toolContent = qobject_cast<ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<ThinkingContent *> ClaudeResponse::getCurrentThinkingContent() const
{
    QList<ThinkingContent *> thinkingBlocks;
    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<ThinkingContent *>(block)) {
            thinkingBlocks.append(thinkingContent);
        }
    }
    return thinkingBlocks;
}

QList<RedactedThinkingContent *> ClaudeResponse::getCurrentRedactedThinkingContent() const
{
    QList<RedactedThinkingContent *> redactedBlocks;
    for (auto block : m_currentBlocks) {
        if (auto redactedContent = qobject_cast<RedactedThinkingContent *>(block)) {
            redactedBlocks.append(redactedContent);
        }
    }
    return redactedBlocks;
}

void ClaudeResponse::startNewContinuation()
{
    LOG_MESSAGE(QString("ClaudeResponse: Starting new continuation"));

    m_currentBlocks.clear();
    m_pendingToolInputs.clear();
    m_stopReason.clear();
    m_state = MessageState::Building;
}

void ClaudeResponse::updateStateFromStopReason()
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
