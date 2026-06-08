// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ResponseRouter.hpp"

#include <LLMQore/ContentBlocks.hpp>

#include <memory>

#include "ConversationHistory.hpp"
#include "Message.hpp"

namespace QodeAssist {

ResponseRouter::ResponseRouter(
    LLMQore::BaseClient *client, ConversationHistory *history, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_history(history)
{
    if (!m_client)
        return;

    connect(
        m_client.data(),
        &LLMQore::BaseClient::chunkReceived,
        this,
        &ResponseRouter::onChunk);
    connect(
        m_client.data(),
        &LLMQore::BaseClient::thinkingBlockReceived,
        this,
        &ResponseRouter::onThinking);
    connect(
        m_client.data(),
        &LLMQore::BaseClient::toolStarted,
        this,
        &ResponseRouter::onToolStarted);
    connect(
        m_client.data(),
        &LLMQore::BaseClient::toolResultReady,
        this,
        &ResponseRouter::onToolResultReady);
    connect(
        m_client.data(),
        &LLMQore::BaseClient::requestFinalized,
        this,
        &ResponseRouter::onFinalized);
    connect(
        m_client.data(),
        &LLMQore::BaseClient::requestFailed,
        this,
        &ResponseRouter::onFailed);
}

ResponseRouter::~ResponseRouter() = default;

void ResponseRouter::beginRequest(const LLMQore::RequestID &id)
{
    m_activeId = id;
    resetTurnState();
}

void ResponseRouter::endRequest()
{
    m_activeId.clear();
    resetTurnState();
}

void ResponseRouter::resetTurnState()
{
    m_assistantOpen = false;
    m_inToolResults = false;
}

void ResponseRouter::ensureAssistantOpen()
{
    if (m_assistantOpen && !m_inToolResults)
        return;
    if (m_history)
        m_history->append(Message(Message::Role::Assistant));
    emit event(ResponseEvent::messageStart());
    m_assistantOpen = true;
    m_inToolResults = false;
}

void ResponseRouter::onChunk(const LLMQore::RequestID &id, const QString &chunk)
{
    if (id != m_activeId || chunk.isEmpty())
        return;
    ensureAssistantOpen();
    if (m_history)
        m_history->appendTextDeltaToLast(chunk);
    emit event(ResponseEvent::textDelta(chunk));
}

void ResponseRouter::onThinking(
    const LLMQore::RequestID &id, const QString &thinking, const QString &signature)
{
    if (id != m_activeId || (thinking.isEmpty() && signature.isEmpty()))
        return;
    ensureAssistantOpen();
    if (m_history)
        m_history->appendThinkingDeltaToLast(thinking, signature);
    emit event(ResponseEvent::thinkingDelta(thinking, signature));
}

void ResponseRouter::onToolStarted(
    const LLMQore::RequestID &id, const QString &toolId, const QString &toolName)
{
    if (id != m_activeId)
        return;
    ensureAssistantOpen();
    if (m_history)
        m_history->appendBlockToLast(
            std::make_unique<LLMQore::ToolUseContent>(toolId, toolName));
    emit event(ResponseEvent::toolCallStart(toolId, toolName));
}

void ResponseRouter::onToolResultReady(
    const LLMQore::RequestID &id,
    const QString &toolId,
    const QString &toolName,
    const QString &result)
{
    Q_UNUSED(toolName);
    if (id != m_activeId)
        return;

    if (m_history) {
        if (m_inToolResults) {
            m_history->appendBlockToLast(
                std::make_unique<LLMQore::ToolResultContent>(toolId, result));
        } else {
            Message m(Message::Role::User);
            m.appendBlock(std::make_unique<LLMQore::ToolResultContent>(toolId, result));
            m_history->append(std::move(m));
        }
    }

    m_assistantOpen = false;
    m_inToolResults = true;
    emit event(ResponseEvent::toolResult(toolId, result, /*isError=*/false));
}

void ResponseRouter::onFinalized(
    const LLMQore::RequestID &id, const LLMQore::CompletionInfo &info)
{
    if (id != m_activeId)
        return;
    emit event(ResponseEvent::messageStop(info.stopReason));
    endRequest();
}

void ResponseRouter::onFailed(const LLMQore::RequestID &id, const QString &err)
{
    if (id != m_activeId)
        return;
    emit event(ResponseEvent::error(err));
    endRequest();
}

} // namespace QodeAssist
