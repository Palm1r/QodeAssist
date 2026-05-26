// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AgentFactory.hpp"

#include <QLoggingCategory>
#include <QThread>

#include <coreplugin/icore.h>

#include "Agent.hpp"
#include "AgentLoader.hpp"
#include "Provider.hpp"
#include "ProviderFactory.hpp"
#include "Logger.hpp"
#include "ProviderSecretsStore.hpp"
#include "ProviderInstance.hpp"
#include "ProviderInstanceFactory.hpp"

static inline void initAgentsResource() { Q_INIT_RESOURCE(agents); }

namespace {
Q_LOGGING_CATEGORY(agentFactoryLog, "qodeassist.agentfactory")

QString agentQrcPrefix() { return QStringLiteral(":/agents"); }
} // namespace

namespace QodeAssist {

AgentFactory::AgentFactory(
    Providers::ProviderInstanceFactory *instanceFactory,
    Providers::ProviderSecretsStore *secrets,
    QObject *parent)
    : QObject(parent)
    , m_instanceFactory(instanceFactory)
    , m_secrets(secrets)
{
    ::initAgentsResource();
    reload();
}

AgentFactory::~AgentFactory() = default;

QString AgentFactory::userAgentsDir()
{
    return Core::ICore::userResourcePath(QStringLiteral("qodeassist/config/agents"))
        .toFSPathString();
}

void AgentFactory::reload()
{
    Q_ASSERT(thread() == QThread::currentThread());
    clear();

    auto result = Agents::AgentLoader::load(agentQrcPrefix(), userAgentsDir());
    for (const QString &err : result.errors)
        LOG_MESSAGE(QString("[Agents] error: %1").arg(err));
    for (const QString &warn : result.warnings)
        LOG_MESSAGE(QString("[Agents] warning: %1").arg(warn));
    LOG_MESSAGE(QString("[Agents] Loaded %1 profiles (qrc=%2, user=%3)")
                    .arg(result.configs.size())
                    .arg(agentQrcPrefix(), userAgentsDir()));

    for (auto &cfg : result.configs) {
        LOG_MESSAGE(QString("[Agents] Loaded: %1").arg(cfg.name));
        registerConfig(std::move(cfg));
    }
    m_errors = std::move(result.errors);
    m_warnings = std::move(result.warnings);
}

void AgentFactory::registerConfig(AgentConfig config)
{
    Q_ASSERT(thread() == QThread::currentThread());

    const QString error = AgentConfig::validate(config);
    if (!error.isEmpty()) {
        qCWarning(agentFactoryLog).noquote() << "Rejected agent config:" << error;
        return;
    }
    const auto it = m_indexByName.constFind(config.name);
    if (it != m_indexByName.constEnd()) {
        m_configs[it.value()] = std::move(config);
        return;
    }
    m_indexByName.insert(config.name, static_cast<qsizetype>(m_configs.size()));
    m_configs.push_back(std::move(config));
}

const AgentConfig *AgentFactory::configByName(const QString &name) const
{
    const auto it = m_indexByName.constFind(name);
    if (it == m_indexByName.constEnd())
        return nullptr;
    return &m_configs[it.value()];
}

QStringList AgentFactory::configNames() const
{
    QStringList out;
    out.reserve(static_cast<qsizetype>(m_configs.size()));
    for (const auto &c : m_configs) {
        if (c.hidden) continue;
        out.append(c.name);
    }
    return out;
}

namespace {

Providers::Provider *buildProviderForAgent(
    const AgentConfig &cfg,
    Providers::ProviderInstanceFactory *instanceFactory,
    Providers::ProviderSecretsStore *secrets,
    QString *errorOut)
{
    if (!instanceFactory) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                            "Agent '%1' cannot be built — no ProviderInstanceFactory was wired "
                            "into AgentFactory")
                            .arg(cfg.name);
        }
        return nullptr;
    }
    const Providers::ProviderInstance *inst
        = instanceFactory->instanceByName(cfg.providerInstance);
    if (!inst) {
        if (errorOut) {
            *errorOut = QStringLiteral(
                            "Agent '%1' references unknown provider instance '%2'")
                            .arg(cfg.name, cfg.providerInstance);
        }
        return nullptr;
    }
    const QString validation = Providers::ProviderInstance::validate(
        *inst, Providers::ProviderFactory::knownNames());
    if (!validation.isEmpty()) {
        if (errorOut)
            *errorOut = validation;
        return nullptr;
    }
    Providers::Provider *provider = Providers::ProviderFactory::create(inst->clientApi, nullptr);
    if (!provider) {
        if (errorOut) {
            *errorOut = QStringLiteral("Client API '%1' is not registered (instance '%2')")
                            .arg(inst->clientApi, inst->name);
        }
        return nullptr;
    }
    provider->setUrl(inst->url);
    if (secrets && !inst->apiKeyRef.isEmpty())
        provider->setApiKey(secrets->readKeySync(inst->apiKeyRef));
    return provider;
}

} // namespace

Agent *AgentFactory::create(const QString &name, QObject *parent, QString *errorOut) const
{
    const AgentConfig *cfg = configByName(name);
    if (!cfg) {
        if (errorOut)
            *errorOut = QStringLiteral("Agent '%1' is not registered").arg(name);
        return nullptr;
    }
    Providers::Provider *provider = buildProviderForAgent(
        *cfg, m_instanceFactory.data(), m_secrets.data(), errorOut);
    if (!provider)
        return nullptr;
    auto agent = std::make_unique<Agent>(*cfg, provider, /*parent=*/nullptr);
    if (!agent->isValid()) {
        if (errorOut)
            *errorOut = agent->invalidReason();
        return nullptr;
    }
    agent->setParent(parent);
    return agent.release();
}

Agent *AgentFactory::createFromFile(
    const QString &tomlPath, QObject *parent, QString *errorOut) const
{
    QString parseErr;
    QStringList warnings;
    auto cfgOpt = Agents::AgentLoader::parseFile(tomlPath, &parseErr, &warnings);
    if (!cfgOpt) {
        if (errorOut) *errorOut = parseErr;
        return nullptr;
    }
    Providers::Provider *provider = buildProviderForAgent(
        *cfgOpt, m_instanceFactory.data(), m_secrets.data(), errorOut);
    if (!provider)
        return nullptr;
    auto agent = std::make_unique<Agent>(std::move(*cfgOpt), provider, /*parent=*/nullptr);
    if (!agent->isValid()) {
        if (errorOut) *errorOut = agent->invalidReason();
        return nullptr;
    }
    agent->setParent(parent);
    return agent.release();
}

void AgentFactory::clear()
{
    Q_ASSERT(thread() == QThread::currentThread());
    m_configs.clear();
    m_indexByName.clear();
    m_errors.clear();
    m_warnings.clear();
}

Providers::ProviderInstanceFactory *AgentFactory::instanceFactory() const noexcept
{
    return m_instanceFactory.data();
}

Providers::ProviderSecretsStore *AgentFactory::secretsStore() const noexcept
{
    return m_secrets.data();
}

} // namespace QodeAssist
