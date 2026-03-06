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

#include "OllamaMessage.hpp"
#include "llmcore/core/Log.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::LLMCore {

OllamaMessage::OllamaMessage(QObject *parent)
    : QObject(parent)
{}

void OllamaMessage::handleContentDelta(const QString &content)
{
    m_accumulatedContent += content;
    QString trimmed = m_accumulatedContent.trimmed();

    if (trimmed.startsWith('{')) {
        return;
    }

    if (!m_contentAddedToTextBlock) {
        TextContent *textContent = getOrCreateTextContent();
        textContent->setText(m_accumulatedContent);
        m_contentAddedToTextBlock = true;
        qCDebug(llmOllamaLog).noquote() << QString("Added accumulated content to TextContent, length=%1")
                                                  .arg(m_accumulatedContent.length());
    } else {
        TextContent *textContent = getOrCreateTextContent();
        textContent->appendText(content);
    }
}

void OllamaMessage::handleToolCall(const QJsonObject &toolCall)
{
    QJsonObject function = toolCall["function"].toObject();
    QString name = function["name"].toString();
    QJsonObject arguments = function["arguments"].toObject();

    QString toolId = QString("call_%1_%2").arg(name).arg(QDateTime::currentMSecsSinceEpoch());

    if (!m_contentAddedToTextBlock && !m_accumulatedContent.trimmed().isEmpty()) {
        qCDebug(llmOllamaLog).noquote() << QString("Clearing accumulated content (tool call detected), length=%1")
                                              .arg(m_accumulatedContent.length());
        m_accumulatedContent.clear();
    }

    addCurrentContent<ToolUseContent>(toolId, name, arguments);

    qCDebug(llmOllamaLog).noquote() << QString("Structured tool call detected - name=%1, id=%2").arg(name, toolId);
}

void OllamaMessage::handleThinkingDelta(const QString &thinking)
{
    ThinkingContent *thinkingContent = getOrCreateThinkingContent();
    thinkingContent->appendThinking(thinking);
}

void OllamaMessage::handleThinkingComplete(const QString &signature)
{
    if (m_currentThinkingContent) {
        m_currentThinkingContent->setSignature(signature);
        qCDebug(llmOllamaLog).noquote() << QString("Set thinking signature, length=%1")
                                                  .arg(signature.length());
    }
}

void OllamaMessage::handleDone(bool done)
{
    m_done = done;
    if (done) {
        bool isToolCall = tryParseToolCall();

        if (!isToolCall && !m_contentAddedToTextBlock && !m_accumulatedContent.trimmed().isEmpty()) {
            QString trimmed = m_accumulatedContent.trimmed();

            if (trimmed.startsWith('{')
                && (trimmed.contains("\"name\"") || trimmed.contains("\"arguments\""))) {
                qCDebug(llmOllamaLog).noquote() << QString("Skipping invalid/incomplete tool call JSON (length=%1)")
                                                          .arg(trimmed.length());

                for (auto it = m_currentBlocks.begin(); it != m_currentBlocks.end();) {
                    if (qobject_cast<TextContent *>(*it)) {
                        qCDebug(llmOllamaLog).noquote() << "Removing TextContent block (incomplete tool call)";
                        (*it)->deleteLater();
                        it = m_currentBlocks.erase(it);
                    } else {
                        ++it;
                    }
                }

                m_accumulatedContent.clear();
            } else {
                TextContent *textContent = getOrCreateTextContent();
                textContent->setText(m_accumulatedContent);
                m_contentAddedToTextBlock = true;
                qCDebug(llmOllamaLog).noquote() << QString("Added final accumulated content to TextContent, length=%1")
                                                          .arg(m_accumulatedContent.length());
            }
        }

        updateStateFromDone();
    }
}
bool OllamaMessage::tryParseToolCall()
{
    QString trimmed = m_accumulatedContent.trimmed();

    if (trimmed.isEmpty()) {
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qCDebug(llmOllamaLog).noquote() << QString("Content is not valid JSON (not a tool call): %1")
                                              .arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        qCDebug(llmOllamaLog).noquote() << "Content is not a JSON object (not a tool call)";
        return false;
    }

    QJsonObject obj = doc.object();

    if (!obj.contains("name") || !obj.contains("arguments")) {
        qCDebug(llmOllamaLog).noquote() << "JSON missing 'name' or 'arguments' fields (not a tool call)";
        return false;
    }

    QString name = obj["name"].toString();
    QJsonValue argsValue = obj["arguments"];
    QJsonObject arguments;

    if (argsValue.isObject()) {
        arguments = argsValue.toObject();
    } else if (argsValue.isString()) {
        QJsonDocument argsDoc = QJsonDocument::fromJson(argsValue.toString().toUtf8());
        if (argsDoc.isObject()) {
            arguments = argsDoc.object();
        } else {
            qCDebug(llmOllamaLog).noquote() << "Failed to parse arguments as JSON object";
            return false;
        }
    } else {
        qCDebug(llmOllamaLog).noquote() << "Arguments field is neither object nor string";
        return false;
    }

    if (name.isEmpty()) {
        qCDebug(llmOllamaLog).noquote() << "Tool name is empty";
        return false;
    }

    QString toolId = QString("call_%1_%2").arg(name).arg(QDateTime::currentMSecsSinceEpoch());

    for (auto block : m_currentBlocks) {
        if (qobject_cast<TextContent *>(block)) {
            qCDebug(llmOllamaLog).noquote() << "Removing TextContent block (tool call detected)";
        }
    }
    m_currentBlocks.clear();

    addCurrentContent<ToolUseContent>(toolId, name, arguments);

    qCDebug(llmOllamaLog).noquote() << QString("Successfully parsed tool call from legacy format - name=%1, id=%2, args=%3")
                                              .arg(name, toolId,
                                                   QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact)));

    return true;
}

bool OllamaMessage::isLikelyToolCallJson(const QString &content) const
{
    QString trimmed = content.trimmed();

    if (trimmed.startsWith('{')) {
        if (trimmed.contains("\"name\"") && trimmed.contains("\"arguments\"")) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);

            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("name") && obj.contains("arguments")) {
                    return true;
                }
            }
        }
    }

    return false;
}

QJsonObject OllamaMessage::toProviderFormat() const
{
    QJsonObject message;
    message["role"] = "assistant";

    QString textContent;
    QJsonArray toolCalls;
    QString thinkingContent;

    for (auto block : m_currentBlocks) {
        if (!block)
            continue;

        if (auto text = qobject_cast<TextContent *>(block)) {
            textContent += text->text();
        } else if (auto tool = qobject_cast<ToolUseContent *>(block)) {
            QJsonObject toolCall;
            toolCall["type"] = "function";
            toolCall["function"] = QJsonObject{{"name", tool->name()}, {"arguments", tool->input()}};
            toolCalls.append(toolCall);
        } else if (auto thinking = qobject_cast<ThinkingContent *>(block)) {
            thinkingContent += thinking->thinking();
        }
    }

    if (!thinkingContent.isEmpty()) {
        message["thinking"] = thinkingContent;
    }

    if (!textContent.isEmpty()) {
        message["content"] = textContent;
    }

    if (!toolCalls.isEmpty()) {
        message["tool_calls"] = toolCalls;
    }

    return message;
}

QJsonArray OllamaMessage::createToolResultMessages(const QHash<QString, QString> &toolResults) const
{
    QJsonArray messages;

    for (auto toolContent : getCurrentToolUseContent()) {
        if (toolResults.contains(toolContent->id())) {
            QJsonObject toolMessage;
            toolMessage["role"] = "tool";
            toolMessage["content"] = toolResults[toolContent->id()];
            messages.append(toolMessage);

            qCDebug(llmOllamaLog).noquote() << QString("Created tool result message for tool %1 (id=%2), content length=%3")
                                                      .arg(toolContent->name(), toolContent->id())
                                                      .arg(toolResults[toolContent->id()].length());
        }
    }

    return messages;
}

QList<ToolUseContent *> OllamaMessage::getCurrentToolUseContent() const
{
    QList<ToolUseContent *> toolBlocks;
    for (auto block : m_currentBlocks) {
        if (auto toolContent = qobject_cast<ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<ThinkingContent *> OllamaMessage::getCurrentThinkingContent() const
{
    QList<ThinkingContent *> thinkingBlocks;
    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<ThinkingContent *>(block)) {
            thinkingBlocks.append(thinkingContent);
        }
    }
    return thinkingBlocks;
}

void OllamaMessage::startNewContinuation()
{
    qCDebug(llmOllamaLog).noquote() << "Starting new continuation";

    m_currentBlocks.clear();
    m_accumulatedContent.clear();
    m_done = false;
    m_state = MessageState::Building;
    m_contentAddedToTextBlock = false;
    m_currentThinkingContent = nullptr;
}

void OllamaMessage::updateStateFromDone()
{
    if (!getCurrentToolUseContent().empty()) {
        m_state = MessageState::RequiresToolExecution;
        qCDebug(llmOllamaLog).noquote() << QString("State set to RequiresToolExecution, tools count=%1")
                                                  .arg(getCurrentToolUseContent().size());
    } else {
        m_state = MessageState::Final;
        qCDebug(llmOllamaLog).noquote() << "State set to Final";
    }
}

TextContent *OllamaMessage::getOrCreateTextContent()
{
    for (auto block : m_currentBlocks) {
        if (auto textContent = qobject_cast<TextContent *>(block)) {
            return textContent;
        }
    }

    return addCurrentContent<TextContent>();
}

ThinkingContent *OllamaMessage::getOrCreateThinkingContent()
{
    if (m_currentThinkingContent) {
        return m_currentThinkingContent;
    }

    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<ThinkingContent *>(block)) {
            m_currentThinkingContent = thinkingContent;
            return m_currentThinkingContent;
        }
    }

    m_currentThinkingContent = addCurrentContent<ThinkingContent>();
    qCDebug(llmOllamaLog).noquote() << "Created new ThinkingContent block";
    return m_currentThinkingContent;
}

} // namespace QodeAssist::LLMCore
