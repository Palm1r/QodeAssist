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

void GoogleMessage::handleFunctionCallStart(const QString &name)
{
    m_currentFunctionName = name;
    m_pendingFunctionArgs.clear();

    LOG_MESSAGE(QString("Google: Starting function call: %1").arg(name));
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

    LOG_MESSAGE(QString("Google: Completed function call: name=%1, args=%2")
                    .arg(m_currentFunctionName)
                    .arg(QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))));

    m_currentFunctionName.clear();
    m_pendingFunctionArgs.clear();
}

void GoogleMessage::handleFinishReason(const QString &reason)
{
    m_finishReason = reason;
    updateStateFromFinishReason();

    LOG_MESSAGE(
        QString("Google: Finish reason: %1, state: %2").arg(reason).arg(static_cast<int>(m_state)));
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

void GoogleMessage::startNewContinuation()
{
    LOG_MESSAGE(QString("GoogleMessage: Starting new continuation"));

    m_currentBlocks.clear();
    m_pendingFunctionArgs.clear();
    m_currentFunctionName.clear();
    m_finishReason.clear();
    m_state = LLMCore::MessageState::Building;
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
