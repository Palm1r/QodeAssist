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

#include "GoogleMessage.hpp"

#include <QJsonDocument>
#include <QUuid>

#include "logger/Logger.hpp"

namespace QodeAssist::Providers {

GoogleMessage::GoogleMessage(QObject *parent)
    : QObject(parent)
{}

void GoogleMessage::handleContentDelta(const QString &text)
{
    if (m_currentBlocks.isEmpty() || !qobject_cast<LLMCore::TextContent *>(m_currentBlocks.last())) {
        auto textContent = new LLMCore::TextContent();
        textContent->setParent(this);
        m_currentBlocks.append(textContent);
    }

    if (auto textContent = qobject_cast<LLMCore::TextContent *>(m_currentBlocks.last())) {
        textContent->appendText(text);
    }
}

void GoogleMessage::handleThoughtDelta(const QString &text)
{
    if (m_currentBlocks.isEmpty() || !qobject_cast<LLMCore::ThinkingContent *>(m_currentBlocks.last())) {
        auto thinkingContent = new LLMCore::ThinkingContent();
        thinkingContent->setParent(this);
        m_currentBlocks.append(thinkingContent);
    }

    if (auto thinkingContent = qobject_cast<LLMCore::ThinkingContent *>(m_currentBlocks.last())) {
        thinkingContent->appendThinking(text);
    }
}

void GoogleMessage::handleThoughtSignature(const QString &signature)
{
    for (int i = m_currentBlocks.size() - 1; i >= 0; --i) {
        if (auto thinkingContent = qobject_cast<LLMCore::ThinkingContent *>(m_currentBlocks[i])) {
            thinkingContent->setSignature(signature);
            return;
        }
    }
    
    auto thinkingContent = new LLMCore::ThinkingContent();
    thinkingContent->setParent(this);
    thinkingContent->setSignature(signature);
    m_currentBlocks.append(thinkingContent);
}

void GoogleMessage::handleFunctionCallStart(const QString &name)
{
    m_currentFunctionName = name;
    m_pendingFunctionArgs.clear();
}

void GoogleMessage::handleFunctionCallArgsDelta(const QString &argsJson)
{
    m_pendingFunctionArgs += argsJson;
}

void GoogleMessage::handleFunctionCallComplete()
{
    if (m_currentFunctionName.isEmpty()) {
        return;
    }

    QJsonObject args;
    if (!m_pendingFunctionArgs.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(m_pendingFunctionArgs.toUtf8());
        if (doc.isObject()) {
            args = doc.object();
        }
    }

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto toolContent = new LLMCore::ToolUseContent(id, m_currentFunctionName, args);
    toolContent->setParent(this);
    m_currentBlocks.append(toolContent);

    m_currentFunctionName.clear();
    m_pendingFunctionArgs.clear();
}

void GoogleMessage::handleFinishReason(const QString &reason)
{
    m_finishReason = reason;
    updateStateFromFinishReason();
}

QJsonObject GoogleMessage::toProviderFormat() const
{
    QJsonObject content;
    content["role"] = "model";

    QJsonArray parts;

    for (auto block : m_currentBlocks) {
        if (!block)
            continue;

        if (auto text = qobject_cast<LLMCore::TextContent *>(block)) {
            parts.append(QJsonObject{{"text", text->text()}});
        } else if (auto tool = qobject_cast<LLMCore::ToolUseContent *>(block)) {
            QJsonObject functionCall;
            functionCall["name"] = tool->name();
            functionCall["args"] = tool->input();
            parts.append(QJsonObject{{"functionCall", functionCall}});
        } else if (auto thinking = qobject_cast<LLMCore::ThinkingContent *>(block)) {
            // Include thinking blocks with their text
            QJsonObject thinkingPart;
            thinkingPart["text"] = thinking->thinking();
            thinkingPart["thought"] = true;
            parts.append(thinkingPart);
            
            // If there's a signature, add it as a separate part
            if (!thinking->signature().isEmpty()) {
                QJsonObject signaturePart;
                signaturePart["thoughtSignature"] = thinking->signature();
                parts.append(signaturePart);
            }
        }
    }

    content["parts"] = parts;
    return content;
}

QJsonArray GoogleMessage::createToolResultParts(const QHash<QString, QString> &toolResults) const
{
    QJsonArray parts;

    for (auto toolContent : getCurrentToolUseContent()) {
        if (toolResults.contains(toolContent->id())) {
            QJsonObject functionResponse;
            functionResponse["name"] = toolContent->name();

            QJsonObject response;
            response["result"] = toolResults[toolContent->id()];
            functionResponse["response"] = response;

            parts.append(QJsonObject{{"functionResponse", functionResponse}});
        }
    }

    return parts;
}

QList<LLMCore::ToolUseContent *> GoogleMessage::getCurrentToolUseContent() const
{
    QList<LLMCore::ToolUseContent *> toolBlocks;
    for (auto block : m_currentBlocks) {
        if (auto toolContent = qobject_cast<LLMCore::ToolUseContent *>(block)) {
            toolBlocks.append(toolContent);
        }
    }
    return toolBlocks;
}

QList<LLMCore::ThinkingContent *> GoogleMessage::getCurrentThinkingContent() const
{
    QList<LLMCore::ThinkingContent *> thinkingBlocks;
    for (auto block : m_currentBlocks) {
        if (auto thinkingContent = qobject_cast<LLMCore::ThinkingContent *>(block)) {
            thinkingBlocks.append(thinkingContent);
        }
    }
    return thinkingBlocks;
}

void GoogleMessage::startNewContinuation()
{
    LOG_MESSAGE(QString("GoogleMessage: Starting new continuation"));

    m_currentBlocks.clear();
    m_pendingFunctionArgs.clear();
    m_currentFunctionName.clear();
    m_finishReason.clear();
    m_state = LLMCore::MessageState::Building;
}

bool GoogleMessage::isErrorFinishReason() const
{
    return m_finishReason == "SAFETY" 
        || m_finishReason == "RECITATION"
        || m_finishReason == "MALFORMED_FUNCTION_CALL"
        || m_finishReason == "PROHIBITED_CONTENT"
        || m_finishReason == "SPII"
        || m_finishReason == "OTHER";
}

QString GoogleMessage::getErrorMessage() const
{
    if (m_finishReason == "SAFETY") {
        return "Response blocked by safety filters";
    } else if (m_finishReason == "RECITATION") {
        return "Response blocked due to recitation of copyrighted content";
    } else if (m_finishReason == "MALFORMED_FUNCTION_CALL") {
        return "Model attempted to call a function with malformed arguments. Please try rephrasing your request or disabling tools.";
    } else if (m_finishReason == "PROHIBITED_CONTENT") {
        return "Response blocked due to prohibited content";
    } else if (m_finishReason == "SPII") {
        return "Response blocked due to sensitive personally identifiable information";
    } else if (m_finishReason == "OTHER") {
        return "Request failed due to an unknown reason";
    }
    return QString();
}

void GoogleMessage::updateStateFromFinishReason()
{
    if (m_finishReason == "STOP" || m_finishReason == "MAX_TOKENS") {
        m_state = getCurrentToolUseContent().isEmpty()
                      ? LLMCore::MessageState::Complete
                      : LLMCore::MessageState::RequiresToolExecution;
    } else {
        m_state = LLMCore::MessageState::Complete;
    }
}

} // namespace QodeAssist::Providers
