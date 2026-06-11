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
    const ErrorInfo &lastError() const noexcept;

    using ContentLoader = std::function<QString(const QString &storedPath)>;
    void setContentLoader(ContentLoader loader);

    Agent *agent() noexcept { return m_agent; }
    ConversationHistory *history() const noexcept { return m_history; }
    SystemPromptBuilder *systemPrompt() const noexcept { return m_systemPrompt; }

    LLMQore::BaseClient *client() const noexcept;
    bool supportsImages() const noexcept;

    void setContextBindings(Templates::ContextRenderer::Bindings bindings);

    LLMQore::RequestID send(
        std::vector<std::unique_ptr<LLMQore::ContentBlock>> userBlocks,
        std::optional<bool> toolsOverride = std::nullopt);

    LLMQore::RequestID sendText(const QString &text);

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
    LLMQore::RequestID dispatch(std::optional<bool> toolsOverride = std::nullopt);
    LLMQore::RequestID dispatchContext(Templates::ContextData ctx, bool tools);
    void teardownInFlight();
    Templates::ContextData toLegacyContext() const;

    Agent *m_agent = nullptr;                              // child if non-null
    QPointer<ConversationHistory> m_history;               // child if internal, external otherwise
    SystemPromptBuilder *m_systemPrompt = nullptr; // child
    ResponseRouter *m_router = nullptr;                    // child, only when valid

    LLMQore::RequestID m_inFlight;
    QString m_invalidReason;
    ErrorInfo m_lastError;

    Templates::ContextRenderer::Bindings m_contextBindings;
    ContentLoader m_contentLoader;

public:
    static Templates::ContextData buildLegacyContext(
        const std::vector<Message> &history,
        const QString &systemPrompt,
        const ContentLoader &loader = ContentLoader{});
};

} // namespace QodeAssist
