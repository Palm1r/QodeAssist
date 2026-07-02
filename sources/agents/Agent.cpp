// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "Agent.hpp"

#include <QThread>

#include "JsonPromptTemplate.hpp"
#include "PromptTemplate.hpp"
#include "Provider.hpp"

namespace QodeAssist {

using Providers::Provider;
using Templates::JsonPromptTemplate;
using Templates::PromptTemplate;

QString AgentConfig::validate(const AgentConfig &config)
{
    if (config.name.isEmpty())
        return QStringLiteral("Agent config has no name");
    if (config.schemaVersion > AgentConfig::kSupportedSchemaVersion) {
        return QStringLiteral(
                   "Agent config '%1' declares schema_version %2 but this plugin "
                   "supports at most %3 — update QodeAssist to use this profile")
            .arg(config.name)
            .arg(config.schemaVersion)
            .arg(AgentConfig::kSupportedSchemaVersion);
    }
    if (config.providerInstance.isEmpty())
        return QStringLiteral("Agent config '%1' has no provider_instance").arg(config.name);
    if (config.model.isEmpty())
        return QStringLiteral("Agent config '%1' has no model").arg(config.name);
    if (config.endpoint.isEmpty())
        return QStringLiteral("Agent config '%1' has no endpoint").arg(config.name);
    if (config.body.isEmpty()) {
        return QStringLiteral("Agent config '%1' has no [body]").arg(config.name);
    }
    return {};
}

Agent::Agent(AgentConfig config, Providers::Provider *providerOwned, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_provider(providerOwned)
{
    m_invalidReason = AgentConfig::validate(m_config);
    if (!m_invalidReason.isEmpty())
        return;

    if (!m_provider) {
        m_invalidReason
            = QStringLiteral("Agent '%1' was constructed without a provider").arg(m_config.name);
        return;
    }
    m_provider->setParent(this);
    m_provider->setPromptCaching(
        m_config.cachePrompt,
        m_config.cacheTtl == QLatin1StringView{"1h"},
        m_config.cacheBreakpoints);

    QString tmplErr;
    m_promptTemplate = JsonPromptTemplate::fromConfig(m_config, &tmplErr);
    if (!m_promptTemplate) {
        m_invalidReason = tmplErr.isEmpty()
                              ? QStringLiteral("Failed to build prompt template for agent '%1'")
                                    .arg(m_config.name)
                              : tmplErr;
    }
}

Agent::~Agent() = default;

PromptTemplate *Agent::promptTemplate() noexcept
{
    return m_promptTemplate.get();
}

const PromptTemplate *Agent::promptTemplate() const noexcept
{
    return m_promptTemplate.get();
}

QFuture<QList<QString>> Agent::installedModels()
{
    Q_ASSERT_X(thread() == QThread::currentThread(), Q_FUNC_INFO,
               "Agent::installedModels called from non-owning thread; "
               "the underlying BaseClient is not thread-safe and must be "
               "accessed from the Agent's owner thread");

    if (!m_provider) {
        return QtFuture::makeReadyValueFuture(QList<QString>{});
    }
    return m_provider->getInstalledModels(m_provider->url());
}

} // namespace QodeAssist
