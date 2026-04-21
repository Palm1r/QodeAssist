// Copyright (C) 2025-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "OllamaMessage.hpp"
#include "logger/Logger.hpp"

#include <QJsonArray>
#include <QJsonDocument>

namespace QodeAssist::Providers {

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
        PluginLLMCore::TextContent *textContent = getOrCreateTextContent();
        textContent->setText(m_accumulatedContent);
        m_contentAddedToTextBlock = true;
        LOG_MESSAGE(QString("OllamaMessage: Added accumulated content to TextContent, length=%1")
                        .arg(m_accumulatedContent.length()));
    } else {
        PluginLLMCore::TextContent *textContent = getOrCreateTextContent();
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
        LOG_MESSAGE(
            QString("OllamaMessage: Clearing accumulated content (tool call detected), length=%1")
                .arg(m_accumulatedContent.length()));
        m_accumulatedContent.clear();
    }

    addCurrentContent<PluginLLMCore::ToolUseContent>(toolId, name, arguments);

    LOG_MESSAGE(
        QString("OllamaMessage: Structured tool call detected - name=%1, id=%2").arg(name, toolId));
}

void OllamaMessage::handleThinkingDelta(const QString &thinking)
{
    PluginLLMCore::ThinkingContent *thinkingContent = getOrCreateThinkingContent();
    thinkingContent->appendThinking(thinking);
}

void OllamaMessage::handleThinkingComplete(const QString &signature)
{
    if (m_currentThinkingContent) {
        m_currentThinkingContent->setSignature(signature);
        LOG_MESSAGE(QString("OllamaMessage: Set thinking signature, length=%1")
                        .arg(signature.length()));
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
                LOG_MESSAGE(
                    QString("OllamaMessage: Skipping invalid/incomplete tool call JSON (length=%1)")
                        .arg(trimmed.length()));

                for (auto it = m_currentBlocks.begin(); it != m_currentBlocks.end();) {
                    if (qobject_cast<PluginLLMCore::TextContent *>(*it)) {
                        LOG_MESSAGE(QString(
                            "OllamaMessage: Removing TextContent block (incomplete tool call)"));
                        (*it)->deleteLater();
                        it = m_currentBlocks.erase(it);
                    } else {
                        ++it;
                    }
                }

                m_accumulatedContent.clear();
            } else {
                PluginLLMCore::TextContent *textContent = getOrCreateTextContent();
                textContent->setText(m_accumulatedContent);
                m_contentAddedToTextBlock = true;
                LOG_MESSAGE(
                    QString(
                        "OllamaMessage: Added final accumulated content to TextContent, length=%1")
                        .arg(m_accumulatedContent.length()));
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
        LOG_MESSAGE(QString("OllamaMessage: Content is not valid JSON (not a tool call): %1")
                        .arg(parseError.errorString()));
        return false;
    }

    if (!doc.isObject()) {
        LOG_MESSAGE(QString("OllamaMessage: Content is not a JSON object (not a tool call)"));
        return false;
    }

    QJsonObject obj = doc.object();

    if (!obj.contains("name") || !obj.contains("arguments")) {
        LOG_MESSAGE(
            QString("OllamaMessage: JSON missing 'name' or 'arguments' fields (not a tool call)"));
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
            LOG_MESSAGE(QString("OllamaMessage: Failed to parse arguments as JSON object"));
            return false;
        }
    } else {
        LOG_MESSAGE(QString("OllamaMessage: Arguments field is neither object nor string"));
        return false;
    }

    if (name.isEmpty()) {
        LOG_MESSAGE(QString("OllamaMessage: Tool name is empty"));
        return false;
    }

    QString toolId = QString("call_%1_%2").arg(name).arg(QDateTime::currentMSecsSinceEpoch());

    for (auto block : m_currentBlocks) {
        if (qobject_cast<PluginLLMCore::TextContent *>(block)) {
            LOG_MESSAGE(QString("OllamaMessage: Removing TextContent block (tool call detected)"));
        }
    }
    m_currentBlocks.clear();

    addCurrentContent<PluginLLMCore::ToolUseContent>(toolId, name, arguments);

    LOG_MESSAGE(
        QString(
            "OllamaMessage: Successfully parsed tool call from legacy format - name=%1, id=%2, "
            "args=%3")
            .arg(
                name,
                toolId,
                QString::fromUtf8(QJsonDocument(arguments).toJson(QJsonDocument::Compact))));

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

        if (auto text = qobject_cast<PluginLLMCore::TextContent *>(block)) {
            textContent += text->text();
        } else if (auto tool = qobject_cast<PluginLLMCore::ToolUseContent *>(block)) {
            QJsonObject toolCall;
            toolCall["type"] = "function";
            toolCall["function"] = QJsonObject{{"name", tool->name()}, {"arguments", tool->input()}};
            toolCalls.append(toolCall);
        } else if (auto thinking = qobject_cast<PluginLLMCore::ThinkingContent *>(block)) {
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

            LOG_MESSAGE(QString(
                            "OllamaMessage: Created tool result message for tool %1 (id=%2), "
                            "content length=%3")
                            .arg(toolContent->name(), toolContent->id())
                            .arg(toolResults[toolContent->id()].length()));
        }
    }

    return messages;
}

QList<PluginLLMCore::ToolUseContent *> OllamaMessage::getCurrentToolUseContent() const
{
    QList<PluginLLMCore::ToolUseContent *> toolBlocks;
    for (auto block : m_currentBlocks) {
        if (auto toolContent = qobject_cast<PluginLLMCore::ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<PluginLLMCore::ThinkingContent *> OllamaMessage::getCurrentThinkingContent() const
{
    QList<PluginLLMCore::ThinkingContent *> thinkingBlocks;
    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<PluginLLMCore::ThinkingContent *>(block)) {
            thinkingBlocks.append(thinkingContent);
        }
    }
    return thinkingBlocks;
}

void OllamaMessage::startNewContinuation()
{
    LOG_MESSAGE(QString("OllamaMessage: Starting new continuation"));

    m_currentBlocks.clear();
    m_accumulatedContent.clear();
    m_done = false;
    m_state = PluginLLMCore::MessageState::Building;
    m_contentAddedToTextBlock = false;
    m_currentThinkingContent = nullptr;
}

void OllamaMessage::updateStateFromDone()
{
    if (!getCurrentToolUseContent().empty()) {
        m_state = PluginLLMCore::MessageState::RequiresToolExecution;
        LOG_MESSAGE(QString("OllamaMessage: State set to RequiresToolExecution, tools count=%1")
                        .arg(getCurrentToolUseContent().size()));
    } else {
        m_state = PluginLLMCore::MessageState::Final;
        LOG_MESSAGE(QString("OllamaMessage: State set to Final"));
    }
}

PluginLLMCore::TextContent *OllamaMessage::getOrCreateTextContent()
{
    for (auto block : m_currentBlocks) {
        if (auto textContent = qobject_cast<PluginLLMCore::TextContent *>(block)) {
            return textContent;
        }
    }

    return addCurrentContent<PluginLLMCore::TextContent>();
}

PluginLLMCore::ThinkingContent *OllamaMessage::getOrCreateThinkingContent()
{
    if (m_currentThinkingContent) {
        return m_currentThinkingContent;
    }

    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<PluginLLMCore::ThinkingContent *>(block)) {
            m_currentThinkingContent = thinkingContent;
            return m_currentThinkingContent;
        }
    }

    m_currentThinkingContent = addCurrentContent<PluginLLMCore::ThinkingContent>();
    LOG_MESSAGE(QString("OllamaMessage: Created new ThinkingContent block"));
    return m_currentThinkingContent;
}

} // namespace QodeAssist::Providers
