// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>

#include <ContextData.hpp>
#include <ContextRenderer.hpp>

#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>
#include <utility>
#include <vector>

#include "ContextAssembler.hpp"
#include "ConversationHistory.hpp"
#include "ErrorInfo.hpp"
#include "ResponseEvent.hpp"

namespace QodeAssist {

class Agent;
class ResponseRouter;
class SystemPromptBuilder;

class Session : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Session)
public:
    Session(
        Agent *agent,
        ConversationHistory *externalHistory = nullptr,
        QObject *parent = nullptr);

    Session(Agent *agent, QObject *parent);

    ~Session() override;

    bool isValid() const noexcept;
    QString invalidReason() const;
    bool isInFlight() const noexcept;
    bool hasAgent() const noexcept;
    bool canSend() const noexcept;
    const ErrorInfo &lastError() const noexcept;

    using ContentLoader = ContextAssembler::ContentLoader;
    void setContentLoader(ContentLoader loader);

    using PinnedProvider = std::function<QString()>;
    void pinContext(const QString &id, PinnedProvider provider);
    void unpinContext(const QString &id);
    void clearPinnedContext();

    Agent *agent() noexcept { return m_agent; }
    void setAgent(Agent *agent);
    ConversationHistory *history() const noexcept { return m_history; }
    bool usesExternalHistory() const noexcept { return m_externalHistory; }
    SystemPromptBuilder *systemPrompt() const noexcept { return m_systemPrompt; }

    LLMQore::BaseClient *client() const noexcept;

    void setContextBindings(Templates::ContextRenderer::Bindings bindings);

    LLMQore::RequestID send(std::vector<std::unique_ptr<LLMQore::ContentBlock>> userBlocks);

    void cancel();

signals:
    void event(const QodeAssist::ResponseEvent &ev);

    void started(const LLMQore::RequestID &id);
    void finished(const LLMQore::RequestID &id, const QString &stopReason);
    void failed(const LLMQore::RequestID &id, const QodeAssist::ErrorInfo &error);
    void cancelled(const LLMQore::RequestID &id);

private slots:
    void onRouterEvent(const QodeAssist::ResponseEvent &ev);

private:
    LLMQore::RequestID dispatch();
    LLMQore::RequestID dispatchContext(const Templates::ContextData &ctx, bool tools);
    void teardownInFlight();
    Templates::ContextData assembleContext() const;
    QVector<ContextAssembler::PinnedBlock> materializePinned() const;
    QJsonObject buildPayload(const Templates::ContextData &ctx, bool tools, QString *errOut) const;

    Agent *m_agent = nullptr;
    QPointer<ConversationHistory> m_history;
    SystemPromptBuilder *m_systemPrompt = nullptr;
    ResponseRouter *m_router = nullptr;
    bool m_externalHistory = false;

    LLMQore::RequestID m_inFlight;
    QString m_invalidReason;
    ErrorInfo m_lastError;

    Templates::ContextRenderer::Bindings m_contextBindings;
    ContentLoader m_contentLoader;
    std::vector<std::pair<QString, PinnedProvider>> m_pinnedProviders;
};

} // namespace QodeAssist
