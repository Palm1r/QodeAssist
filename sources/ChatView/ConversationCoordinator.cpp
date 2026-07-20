// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ConversationCoordinator.hpp"

#include "logger/Logger.hpp"

namespace QodeAssist::Chat {

ConversationCoordinator::ConversationCoordinator(const Ports &ports, QObject *parent)
    : QObject(parent)
    , m_ports(ports)
{}

void ConversationCoordinator::requestSend(
    const QString &message, const QStringList &attachments, bool useTools, bool useThinking)
{
    if (refuseWhileReadOnly())
        return;

    if (deferSendForAutoCompress(message, attachments, useTools, useThinking))
        return;

    m_ports.send->dispatch(message, attachments, useTools, useThinking);
}

bool ConversationCoordinator::refuseWhileReadOnly()
{
    if (m_sessionIssue.isEmpty())
        return false;

    emit errorSurfaced(m_sessionIssue);
    return true;
}

bool ConversationCoordinator::hasDeferredSend() const
{
    return m_deferredSend.active;
}

bool ConversationCoordinator::deferSendForAutoCompress(
    const QString &message, const QStringList &attachments, bool useTools, bool useThinking)
{
    if (!m_ports.ops->boundAgentId().isEmpty())
        return false;

    if (!m_ports.send->autoCompressEnabled())
        return false;

    const int threshold = m_ports.send->autoCompressThreshold();
    const int inputTokens = m_ports.send->estimatedNextTokens();
    if (inputTokens < threshold)
        return false;

    if (!m_ports.send->prepareChatFileForCompression(message, attachments))
        return false;

    if (m_ports.compression->isCompressionRunning() || m_deferredSend.active)
        return false;

    LOG_MESSAGE(QString("Auto-compress preempt: estimated next=%1 ≥ threshold=%2; deferring send")
                    .arg(inputTokens)
                    .arg(threshold));

    m_deferredSend = {message, attachments, useTools, useThinking, true};
    m_ports.compression->startCompression();
    return true;
}

void ConversationCoordinator::compressionSettled()
{
    if (!m_deferredSend.active)
        return;

    const DeferredSend deferred = m_deferredSend;
    m_deferredSend = {};

    if (refuseWhileReadOnly())
        return;

    m_ports.send->dispatch(
        deferred.message, deferred.attachments, deferred.useTools, deferred.useThinking);
}

void ConversationCoordinator::chooseAgent(const Acp::AgentDefinition &agent)
{
    if (m_ports.ops->boundAgentId() == agent.id)
        return;

    if (m_ports.ops->conversationStarted()) {
        m_pendingAgent = agent;
        m_pendingLlmSwitch = false;
        emit switchConfirmationNeeded(agent.name);
        return;
    }

    bindAgentNow(agent);
}

void ConversationCoordinator::chooseLlm()
{
    if (m_ports.ops->boundAgentId().isEmpty())
        return;

    if (m_ports.ops->conversationStarted()) {
        m_pendingAgent.reset();
        m_pendingLlmSwitch = true;
        emit switchConfirmationNeeded(tr("direct LLM chat"));
        return;
    }

    bindLlmNow();
}

void ConversationCoordinator::confirmSwitch()
{
    const auto agent = m_pendingAgent;
    const bool toLlm = m_pendingLlmSwitch;

    m_pendingAgent.reset();
    m_pendingLlmSwitch = false;

    if (!agent && !toLlm)
        return;

    m_ports.ops->clearConversation();

    if (agent)
        bindAgentNow(*agent);
    else
        bindLlmNow();
}

void ConversationCoordinator::cancelSwitch()
{
    m_pendingAgent.reset();
    m_pendingLlmSwitch = false;
    emit switchCancelled();
}

bool ConversationCoordinator::switchPending() const
{
    return m_pendingAgent.has_value() || m_pendingLlmSwitch;
}

void ConversationCoordinator::bindAgentNow(const Acp::AgentDefinition &agent)
{
    m_ports.ops->bindAgent(agent);
    emit boundToAgent(agent);
}

void ConversationCoordinator::bindLlmNow()
{
    m_ports.ops->bindLlm();
    emit boundToLlm();
}

void ConversationCoordinator::restoreAgentBinding(const Acp::AgentBinding &binding)
{
    setSessionIssue(QString(), false);
    m_quarantinedBinding = {};
    m_ports.ops->releaseAgentSession();

    if (binding.isEmpty()) {
        bindLlmNow();
        return;
    }

    const auto agent = m_ports.catalog->agentById(binding.agentId);
    if (!agent || !agent->isLaunchable()) {
        m_quarantinedBinding = binding;
        bindLlmNow();
        setSessionIssue(
            tr("This chat was held with the agent \"%1\", which is not available any more. "
               "The transcript is read-only, and the agent it names is kept so the chat can be "
               "continued once that agent is installed again.")
                .arg(binding.displayId()),
            false);
        return;
    }

    bindAgentNow(*agent);

    if (!binding.sessionId.isEmpty()) {
        m_ports.ops->resumeAgentSession(binding.sessionId);
        return;
    }

    if (m_ports.ops->conversationStarted()) {
        setSessionIssue(
            tr("This chat records the agent \"%1\" but not a session to reopen, so the transcript "
               "is read-only. Start a new session to keep working with this agent — it will not "
               "have the context above.")
                .arg(binding.displayId()),
            true);
    }
}

Acp::AgentBinding ConversationCoordinator::bindingForSave() const
{
    return m_quarantinedBinding.isEmpty() ? m_ports.ops->agentBinding() : m_quarantinedBinding;
}

QString ConversationCoordinator::sessionIssue() const
{
    return m_sessionIssue;
}

bool ConversationCoordinator::readOnly() const
{
    return !m_sessionIssue.isEmpty();
}

bool ConversationCoordinator::canStartFreshSession() const
{
    return m_sessionRecoverable;
}

bool ConversationCoordinator::canHandOverSummary() const
{
    return m_sessionRecoverable && m_ports.compression->compressionConfigurationIssue().isEmpty()
           && !m_ports.ops->transcriptEmpty();
}

QString ConversationCoordinator::summaryHandoverTooltip() const
{
    if (!m_sessionRecoverable)
        return {};

    if (m_ports.ops->transcriptEmpty())
        return tr("There is nothing to summarise yet.");

    const QString issue = m_ports.compression->compressionConfigurationIssue();
    if (!issue.isEmpty()) {
        return tr("A summary cannot be produced because %1. Start a new session without one, or "
                  "assign the chat feature in the settings.")
            .arg(issue);
    }

    return tr("Summarise this transcript and give it to the new session as context.");
}

void ConversationCoordinator::startFreshSession()
{
    if (!m_sessionRecoverable)
        return;

    m_ports.ops->startFreshAgentSession();
    setSessionIssue(QString(), false);
}

void ConversationCoordinator::handOverSummary()
{
    if (!canHandOverSummary())
        return;

    if (m_ports.compression->isCompressionRunning()) {
        emit errorSurfaced(tr("A summary is already being prepared"));
        return;
    }

    m_ports.compression->startTranscriptSummary();
}

void ConversationCoordinator::summaryProduced(const QString &summary)
{
    m_ports.ops->startFreshAgentSession(summary);
    setSessionIssue(QString(), false);
}

QString ConversationCoordinator::agentTitle() const
{
    return m_agentTitle;
}

void ConversationCoordinator::agentTitleSuggested(const QString &title)
{
    if (m_agentTitle == title)
        return;

    m_agentTitle = title;
    emit agentTitleChanged();
}

void ConversationCoordinator::agentSessionUnavailable(const QString &reason)
{
    LOG_MESSAGE(QString("Agent session could not be reopened: %1").arg(reason));
    setSessionIssue(
        tr("The previous session with this agent could not be reopened, so the transcript "
           "is read-only. Start a new session to keep working with this agent — it will "
           "not have the context above."),
        true);
}

void ConversationCoordinator::conversationReset()
{
    if (!m_agentTitle.isEmpty()) {
        m_agentTitle.clear();
        emit agentTitleChanged();
    }
    setSessionIssue(QString(), false);
}

void ConversationCoordinator::setSessionIssue(const QString &issue, bool recoverable)
{
    if (m_sessionIssue == issue && m_sessionRecoverable == recoverable)
        return;

    m_sessionIssue = issue;
    m_sessionRecoverable = recoverable;
    emit sessionIssueChanged();
}

} // namespace QodeAssist::Chat
