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

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "ConversationHistory.hpp"
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
    explicit Session(QObject *parent = nullptr);

    Session(
        Agent *agent,
        ConversationHistory *externalHistory = nullptr,
        QObject *parent = nullptr);

    Session(Agent *agent, QObject *parent);

    ~Session() override;

    bool isValid() const noexcept;
    QString invalidReason() const;
    bool isInFlight() const noexcept;

    using ContentLoader = std::function<QString(const QString &storedPath)>;
    void setContentLoader(ContentLoader loader);

    Agent *agent() noexcept { return m_agent; }
    ConversationHistory *history() const noexcept { return m_history; }
    SystemPromptBuilder *systemPrompt() const noexcept { return m_systemPrompt; }

    void setContextBindings(Templates::ContextRenderer::Bindings bindings);

    QString renderAgentContext() const;

    LLMQore::RequestID send(
        std::vector<std::unique_ptr<LLMQore::ContentBlock>> userBlocks,
        std::optional<bool> toolsOverride = std::nullopt);

    LLMQore::RequestID sendText(const QString &text);

    LLMQore::RequestID sendCompletion(Templates::ContextData ctx);
    
    void cancel();

signals:
    void event(const QodeAssist::ResponseEvent &ev);

    void started(const LLMQore::RequestID &id);
    void finished(const LLMQore::RequestID &id, const QString &stopReason);
    void failed(const LLMQore::RequestID &id, const QString &error);

private slots:
    void onRouterEvent(const QodeAssist::ResponseEvent &ev);

private:
    LLMQore::RequestID dispatch(std::optional<bool> toolsOverride = std::nullopt);
    Templates::ContextData toLegacyContext() const;

    Agent *m_agent = nullptr;                              // child if non-null
    QPointer<ConversationHistory> m_history;               // child if internal, external otherwise
    SystemPromptBuilder *m_systemPrompt = nullptr; // child
    ResponseRouter *m_router = nullptr;                    // child, only when valid

    LLMQore::RequestID m_inFlight;
    QString m_invalidReason;

    Templates::ContextRenderer::Bindings m_contextBindings;

public:
    static Templates::ContextData buildLegacyContext(
        const std::vector<Message> &history,
        const QString &systemPrompt,
        const ContentLoader &loader = ContentLoader{});

private:
    ContentLoader m_contentLoader;
};

} // namespace QodeAssist
