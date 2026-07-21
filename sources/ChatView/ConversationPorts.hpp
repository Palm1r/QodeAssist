// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "acp/AgentBinding.hpp"
#include "acp/AgentDefinition.hpp"

namespace QodeAssist::Chat {

class IConversationPort
{
public:
    virtual ~IConversationPort() = default;

    virtual QString boundAgentId() const = 0;
    virtual bool conversationStarted() const = 0;
    virtual bool transcriptEmpty() const = 0;
    virtual Acp::AgentBinding agentBinding() const = 0;

    virtual void bindAgent(const Acp::AgentDefinition &agent) = 0;
    virtual void bindLlm() = 0;
    virtual void clearConversation() = 0;

    virtual void resumeAgentSession(const QString &sessionId) = 0;
    virtual void startFreshAgentSession() = 0;
    virtual void startFreshAgentSession(const QString &handoverSummary) = 0;
    virtual void releaseAgentSession() = 0;
};

class IAgentCatalogPort
{
public:
    virtual ~IAgentCatalogPort() = default;

    virtual std::optional<Acp::AgentDefinition> agentById(const QString &agentId) const = 0;
};

class ICompressionPort
{
public:
    virtual ~ICompressionPort() = default;

    virtual QString compressionConfigurationIssue() const = 0;
    virtual bool isCompressionRunning() const = 0;
    virtual void startTranscriptSummary() = 0;
    virtual void startCompression() = 0;
};

class ISendPort
{
public:
    virtual ~ISendPort() = default;

    virtual bool autoCompressEnabled() const = 0;
    virtual int autoCompressThreshold() const = 0;
    virtual int estimatedNextTokens() const = 0;
    virtual bool prepareChatFileForCompression(
        const QString &message, const QStringList &attachments)
        = 0;
    virtual void dispatch(const QString &message, const QStringList &attachments) = 0;
};

} // namespace QodeAssist::Chat
