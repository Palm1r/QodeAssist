// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "Session.hpp"

#include <LLMQore/BaseClient.hpp>

#include <QDebug>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>

#include "Agent.hpp"
#include "AgentConfig.hpp"
#include "ContextData.hpp"
#include "Message.hpp"
#include "PluginBlocks.hpp"
#include "PromptTemplate.hpp"
#include "Provider.hpp"
#include "ResponseRouter.hpp"
#include "SystemPromptBuilder.hpp"

namespace QodeAssist {

namespace {

QString roleToLegacyString(Message::Role role)
{
    switch (role) {
    case Message::Role::System: return QStringLiteral("system");
    case Message::Role::User: return QStringLiteral("user");
    case Message::Role::Assistant: return QStringLiteral("assistant");
    }
    return QStringLiteral("user");
}

[[maybe_unused]] const int kErrorInfoMetaTypeId = qRegisterMetaType<QodeAssist::ErrorInfo>();

} // namespace

Session::Session(Agent *agent, QObject *parent)
    : Session(agent, /*externalHistory=*/nullptr, parent)
{}

Session::Session(Agent *agent, ConversationHistory *externalHistory, QObject *parent)
    : QObject(parent)
    , m_agent(agent)
    , m_history(externalHistory ? externalHistory : new ConversationHistory(this))
    , m_systemPrompt(new SystemPromptBuilder(this))
{
    if (!m_agent) {
        m_invalidReason = QStringLiteral("Session: agent is null");
        return;
    }
    m_agent->setParent(this);

    if (!m_agent->isValid()) {
        m_invalidReason = m_agent->invalidReason();
        return;
    }

    auto *provider = m_agent->provider();
    auto *client = provider ? provider->client() : nullptr;
    if (!client) {
        m_invalidReason = QStringLiteral("Session: provider has no live client");
        return;
    }
    if (!m_agent->promptTemplate()) {
        m_invalidReason
            = QStringLiteral("Session: agent has no inline prompt template");
        return;
    }

    m_router = new ResponseRouter(client, m_history, this);
    connect(m_router, &ResponseRouter::event, this, &Session::onRouterEvent);
}

Session::~Session()
{
    if (isInFlight())
        teardownInFlight();
}

bool Session::isValid() const noexcept
{
    return m_invalidReason.isEmpty();
}

QString Session::invalidReason() const
{
    return m_invalidReason;
}

bool Session::isInFlight() const noexcept
{
    return !m_inFlight.isEmpty();
}

const ErrorInfo &Session::lastError() const noexcept
{
    return m_lastError;
}

LLMQore::BaseClient *Session::client() const noexcept
{
    auto *provider = m_agent ? m_agent->provider() : nullptr;
    return provider ? provider->client() : nullptr;
}

bool Session::supportsImages() const noexcept
{
    auto *provider = m_agent ? m_agent->provider() : nullptr;
    return provider
           && provider->capabilities().testFlag(Providers::ProviderCapability::Image);
}

void Session::setContentLoader(ContentLoader loader)
{
    m_contentLoader = std::move(loader);
}

void Session::setContextBindings(Templates::ContextRenderer::Bindings bindings)
{
    m_contextBindings = std::move(bindings);
}

LLMQore::RequestID Session::sendText(const QString &text)
{
    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    if (!text.isEmpty())
        blocks.push_back(std::make_unique<LLMQore::TextContent>(text));
    return send(std::move(blocks));
}

LLMQore::RequestID Session::sendCompletion(Templates::ContextData ctx)
{
    if (!isValid()) {
        m_lastError = makeError(ErrorCategory::Config, invalidReason());
        return {};
    }
    if (isInFlight())
        cancel();
    return dispatchContext(std::move(ctx), /*tools=*/false);
}

LLMQore::RequestID Session::send(
    std::vector<std::unique_ptr<LLMQore::ContentBlock>> userBlocks,
    std::optional<bool> toolsOverride)
{
    if (!isValid()) {
        m_lastError = makeError(ErrorCategory::Config, invalidReason());
        return {};
    }
    if (userBlocks.empty() || !m_history) {
        m_lastError = makeError(ErrorCategory::Validation, QStringLiteral("Session: nothing to send"));
        return {};
    }

    if (isInFlight())
        cancel();

    Message msg(Message::Role::User);
    for (auto &b : userBlocks)
        msg.appendBlock(std::move(b));
    m_history->append(std::move(msg));

    return dispatch(toolsOverride);
}

void Session::cancel()
{
    if (m_inFlight.isEmpty())
        return;

    const auto id = m_inFlight;
    teardownInFlight();
    emit cancelled(id);
}

void Session::teardownInFlight()
{
    if (m_inFlight.isEmpty())
        return;

    const auto id = m_inFlight;
    m_inFlight.clear();
    if (m_router)
        m_router->endRequest();
    if (m_agent && m_agent->provider())
        m_agent->provider()->cancelRequest(id);
}

LLMQore::RequestID Session::dispatch(std::optional<bool> toolsOverride)
{
    const auto &cfg = m_agent->config();

    if (cfg.systemPrompt.isEmpty()) {
        m_systemPrompt->clearLayer(QStringLiteral("agent.system"));
    } else {
        QString renderErr;
        const QString renderedContext = Templates::ContextRenderer::render(
            cfg.systemPrompt, m_contextBindings, &renderErr);
        if (!renderErr.isEmpty()) {
            m_lastError = makeError(
                ErrorCategory::Validation,
                QStringLiteral("Agent '%1' system_prompt render failed: %2")
                    .arg(cfg.name, renderErr));
            qWarning("[QodeAssist] %s", qUtf8Printable(m_lastError.message));
            return {};
        }
        if (renderedContext.isEmpty())
            m_systemPrompt->clearLayer(QStringLiteral("agent.system"));
        else
            m_systemPrompt->setLayer(
                QStringLiteral("agent.system"), renderedContext, SystemPromptBuilder::kAgentPriority);
    }

    const bool tools = toolsOverride.value_or(cfg.enableTools);
    return dispatchContext(toLegacyContext(), tools);
}

LLMQore::RequestID Session::dispatchContext(Templates::ContextData ctx, bool tools)
{
    m_lastError = {};

    auto *provider = m_agent->provider();
    auto *tmpl = m_agent->promptTemplate();
    const auto &cfg = m_agent->config();

    QJsonObject payload{{QStringLiteral("model"), cfg.model}};
    QString prepareErr;
    if (!provider->prepareRequest(payload, tmpl, ctx, tools, &prepareErr)) {
        m_lastError = makeError(ErrorCategory::Validation, prepareErr, prepareErr);
        return {};
    }

    QString endpoint = cfg.endpoint;
    endpoint.replace(QStringLiteral("${MODEL}"), cfg.model);
    const auto id = provider->sendRequest(QUrl(provider->url()), payload, endpoint);
    if (id.isEmpty()) {
        m_lastError = makeError(
            ErrorCategory::Provider,
            QStringLiteral("Provider '%1' failed to start the request").arg(provider->name()));
        return {};
    }

    m_inFlight = id;
    if (m_router)
        m_router->beginRequest(id);
    emit started(id);
    return id;
}

Templates::ContextData Session::toLegacyContext() const
{
    if (!m_history)
        return {};
    return buildLegacyContext(m_history->messages(), m_systemPrompt->compose(), m_contentLoader);
}

Templates::ContextData Session::buildLegacyContext(
    const std::vector<Message> &history,
    const QString &systemPrompt,
    const ContentLoader &loader)
{
    using Templates::ContentBlockEntry;
    using Templates::ContextData;
    using LegacyMessage = Templates::Message;

    ContextData ctx;
    if (!systemPrompt.isEmpty())
        ctx.systemPrompt = systemPrompt;

    QSet<QString> resolvedToolUseIds;
    QSet<QString> declaredToolUseIds;
    for (const auto &m : history) {
        for (const auto &blockPtr : m.blocks()) {
            if (auto *tr = dynamic_cast<LLMQore::ToolResultContent *>(blockPtr.get()))
                resolvedToolUseIds.insert(tr->toolUseId());
            if (auto *tu = dynamic_cast<LLMQore::ToolUseContent *>(blockPtr.get()))
                declaredToolUseIds.insert(tu->id());
        }
    }

    QVector<LegacyMessage> hist;

    for (const auto &m : history) {
        if (m.role() == Message::Role::System)
            continue;

        QVector<ContentBlockEntry> blockEntries;

        for (const auto &blockPtr : m.blocks()) {
            auto *block = blockPtr.get();
            if (!block)
                continue;

            if (auto *t = dynamic_cast<LLMQore::TextContent *>(block)) {
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Text;
                e.text = t->text();
                blockEntries.append(std::move(e));
            } else if (auto *img = dynamic_cast<LLMQore::ImageContent *>(block)) {
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Image;
                e.imageData = img->data();
                e.mediaType = img->mediaType();
                e.isImageUrl
                    = (img->sourceType() == LLMQore::ImageContent::ImageSourceType::Url);
                blockEntries.append(std::move(e));
            } else if (auto *si = dynamic_cast<StoredImageContent *>(block)) {
                if (!loader)
                    continue;
                const QString base64 = loader(si->storedPath());
                if (base64.isEmpty())
                    continue;
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Image;
                e.imageData = base64;
                e.mediaType = si->mediaType();
                e.isImageUrl = false;
                blockEntries.append(std::move(e));
            } else if (auto *sa = dynamic_cast<StoredAttachmentContent *>(block)) {
                if (!loader)
                    continue;
                const QString text = loader(sa->storedPath());
                if (text.isEmpty())
                    continue;
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Text;
                e.text = QStringLiteral("File: %1\n```\n%2\n```")
                             .arg(sa->fileName(), text);
                blockEntries.append(std::move(e));
            } else if (auto *th = dynamic_cast<LLMQore::ThinkingContent *>(block)) {
                // Claude rejects thinking blocks replayed without a signature.
                if (th->signature().isEmpty())
                    continue;
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::Thinking;
                e.thinking = th->thinking();
                e.signature = th->signature();
                blockEntries.append(std::move(e));
            } else if (auto *rth = dynamic_cast<LLMQore::RedactedThinkingContent *>(block)) {
                if (rth->signature().isEmpty())
                    continue;
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::RedactedThinking;
                e.signature = rth->signature();
                blockEntries.append(std::move(e));
            } else if (auto *tu = dynamic_cast<LLMQore::ToolUseContent *>(block)) {
                if (!resolvedToolUseIds.contains(tu->id()))
                    continue;
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::ToolUse;
                e.toolUseId = tu->id();
                e.toolName = tu->name();
                e.toolInput = tu->input();
                blockEntries.append(std::move(e));
            } else if (auto *tr = dynamic_cast<LLMQore::ToolResultContent *>(block)) {
                if (!declaredToolUseIds.contains(tr->toolUseId()))
                    continue;
                ContentBlockEntry e;
                e.kind = ContentBlockEntry::Kind::ToolResult;
                e.toolUseId = tr->toolUseId();
                e.result = tr->result();
                blockEntries.append(std::move(e));
            }
        }

        if (blockEntries.isEmpty())
            continue;

        const bool hasNonThinking = std::any_of(
            blockEntries.begin(), blockEntries.end(), [](const ContentBlockEntry &e) {
                return e.kind != ContentBlockEntry::Kind::Thinking
                       && e.kind != ContentBlockEntry::Kind::RedactedThinking;
            });
        if (!hasNonThinking)
            continue;

        LegacyMessage lm;
        lm.role = roleToLegacyString(m.role());
        lm.blocks = std::move(blockEntries);
        hist.append(std::move(lm));
    }

    if (!hist.isEmpty())
        ctx.history = std::move(hist);

    return ctx;
}

void Session::onRouterEvent(const ResponseEvent &ev)
{
    if (m_inFlight.isEmpty())
        return; // stale events after cancel

    emit event(ev);

    if (ev.kind() == ResponseEvent::Kind::MessageStop) {
        const auto *stop = ev.as<ResponseEvents::MessageStop>();
        const QString reason = stop ? stop->stopReason : QString();
        const auto id = m_inFlight;
        m_inFlight.clear();
        emit finished(id, reason);
    } else if (ev.kind() == ResponseEvent::Kind::Error) {
        const auto *err = ev.as<ResponseEvents::Error>();
        const QString msg = err ? err->message : QStringLiteral("unknown error");
        const ErrorCategory category = err ? err->category : ErrorCategory::Provider;
        m_lastError = makeError(category, msg, msg);
        const auto id = m_inFlight;
        m_inFlight.clear();
        emit failed(id, m_lastError);
    }
}

} // namespace QodeAssist
