// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "Session.hpp"

#include <LLMQore/BaseClient.hpp>

#include <QDebug>
#include <QJsonObject>
#include <QUrl>

#include "Agent.hpp"
#include "AgentConfig.hpp"
#include "ContextData.hpp"
#include "Message.hpp"
#include "PromptTemplate.hpp"
#include "Provider.hpp"
#include "ResponseRouter.hpp"
#include "SystemPromptBuilder.hpp"

namespace QodeAssist {

namespace {

[[maybe_unused]] const int kErrorInfoMetaTypeId = qRegisterMetaType<QodeAssist::ErrorInfo>();

} // namespace

Session::Session(Agent *agent, QObject *parent)
    : Session(agent, /*externalHistory=*/nullptr, parent)
{}

Session::Session(Agent *agent, ConversationHistory *externalHistory, QObject *parent)
    : QObject(parent)
    , m_history(externalHistory ? externalHistory : new ConversationHistory(this))
    , m_systemPrompt(new SystemPromptBuilder(this))
{
    if (agent)
        setAgent(agent);
}

void Session::setAgent(Agent *agent)
{
    if (agent == m_agent)
        return;

    if (isInFlight())
        teardownInFlight();

    if (m_router) {
        delete m_router;
        m_router = nullptr;
    }

    delete m_agent;
    m_agent = agent;
    m_invalidReason.clear();

    if (!m_agent)
        return;

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
        m_invalidReason = QStringLiteral("Session: agent has no inline prompt template");
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

bool Session::hasAgent() const noexcept
{
    return m_agent != nullptr;
}

bool Session::canSend() const noexcept
{
    return isValid() && m_agent != nullptr && client() != nullptr;
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

void Session::setContentLoader(ContentLoader loader)
{
    m_contentLoader = std::move(loader);
}

void Session::setContextBindings(Templates::ContextRenderer::Bindings bindings)
{
    m_contextBindings = std::move(bindings);
}

void Session::pinContext(const QString &id, PinnedProvider provider)
{
    if (!provider) {
        unpinContext(id);
        return;
    }
    for (auto &entry : m_pinnedProviders) {
        if (entry.first == id) {
            entry.second = std::move(provider);
            return;
        }
    }
    m_pinnedProviders.emplace_back(id, std::move(provider));
}

void Session::unpinContext(const QString &id)
{
    std::erase_if(m_pinnedProviders, [&id](const auto &entry) { return entry.first == id; });
}

LLMQore::RequestID Session::send(std::vector<std::unique_ptr<LLMQore::ContentBlock>> userBlocks)
{
    if (!canSend()) {
        const QString reason = m_agent ? (invalidReason().isEmpty()
                                              ? QStringLiteral("Session: agent has no live client")
                                              : invalidReason())
                                       : QStringLiteral("Session: no agent bound");
        m_lastError = makeError(ErrorCategory::Config, reason);
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

    return dispatch();
}

QVector<ContextAssembler::PinnedBlock> Session::materializePinned() const
{
    QVector<ContextAssembler::PinnedBlock> pinned;
    pinned.reserve(static_cast<qsizetype>(m_pinnedProviders.size()));
    for (const auto &entry : m_pinnedProviders) {
        const QString text = entry.second();
        if (!text.isEmpty())
            pinned.append({entry.first, text});
    }
    return pinned;
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

LLMQore::RequestID Session::dispatch()
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

    return dispatchContext(assembleContext(), cfg.enableTools);
}

LLMQore::RequestID Session::dispatchContext(const Templates::ContextData &ctx, bool tools)
{
    m_lastError = {};

    auto *provider = m_agent->provider();
    const auto &cfg = m_agent->config();

    QString prepareErr;
    const QJsonObject payload = buildPayload(ctx, tools, &prepareErr);
    if (payload.isEmpty()) {
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

QJsonObject Session::buildPayload(
    const Templates::ContextData &ctx, bool tools, QString *errOut) const
{
    auto *provider = m_agent->provider();
    auto *tmpl = m_agent->promptTemplate();
    const auto &cfg = m_agent->config();

    QJsonObject payload{{QStringLiteral("model"), cfg.model}};
    QString prepareErr;
    if (!provider->prepareRequest(payload, tmpl, ctx, tools, &prepareErr)) {
        if (errOut)
            *errOut = prepareErr;
        return {};
    }
    return payload;
}

Templates::ContextData Session::assembleContext() const
{
    if (!m_history)
        return {};
    return ContextAssembler::assemble(
        m_history->messages(), m_systemPrompt->compose(), m_contentLoader, materializePinned());
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
