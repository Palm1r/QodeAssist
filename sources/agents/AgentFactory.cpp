// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentFactory.hpp"

#include <QDir>
#include <QLoggingCategory>
#include <QStringList>
#include <QThread>

#include <coreplugin/icore.h>
#include <utils/qtcsettings.h>

#include "Agent.hpp"
#include "AgentLoader.hpp"
#include "Logger.hpp"
#include "Provider.hpp"
#include "ProviderFactory.hpp"
#include "ProviderInstance.hpp"
#include "ProviderInstanceFactory.hpp"
#include "ProviderSecretsStore.hpp"

static inline void initAgentsResource()
{
    Q_INIT_RESOURCE(agents);
}

namespace {
Q_LOGGING_CATEGORY(agentFactoryLog, "qodeassist.agentfactory")

QString agentQrcPrefix()
{
    return QStringLiteral(":/agents");
}

constexpr auto kModelOverrideGroup = "QodeAssist/AgentModelOverrides";

Utils::Key modelOverrideKey(const QString &name)
{
    return Utils::Key(
        QStringLiteral("%1/%2").arg(QLatin1StringView(kModelOverrideGroup), name).toUtf8());
}

QString readModelOverride(const QString &name)
{
    auto *s = Core::ICore::settings();
    if (!s)
        return {};
    return s->value(modelOverrideKey(name)).toString();
}

void writeModelOverride(const QString &name, const QString &model)
{
    auto *s = Core::ICore::settings();
    if (!s)
        return;
    if (model.isEmpty())
        s->remove(modelOverrideKey(name));
    else
        s->setValue(modelOverrideKey(name), model);
    s->sync();
}

constexpr auto kProviderOverrideGroup = "QodeAssist/AgentProviderOverrides";

Utils::Key providerOverrideKey(const QString &name)
{
    return Utils::Key(
        QStringLiteral("%1/%2").arg(QLatin1StringView(kProviderOverrideGroup), name).toUtf8());
}

QString readProviderOverride(const QString &name)
{
    auto *s = Core::ICore::settings();
    if (!s)
        return {};
    return s->value(providerOverrideKey(name)).toString();
}

void writeProviderOverride(const QString &name, const QString &providerInstance)
{
    auto *s = Core::ICore::settings();
    if (!s)
        return;
    if (providerInstance.isEmpty())
        s->remove(providerOverrideKey(name));
    else
        s->setValue(providerOverrideKey(name), providerInstance);
    s->sync();
}

constexpr auto kToolsOverrideGroup = "QodeAssist/AgentToolsOverrides";

Utils::Key toolsOverrideKey(const QString &name)
{
    return Utils::Key(
        QStringLiteral("%1/%2").arg(QLatin1StringView(kToolsOverrideGroup), name).toUtf8());
}

std::optional<bool> readToolsOverride(const QString &name)
{
    auto *s = Core::ICore::settings();
    if (!s)
        return std::nullopt;
    const Utils::Key key = toolsOverrideKey(name);
    if (!s->contains(key))
        return std::nullopt;
    return s->value(key).toBool();
}

void writeToolsOverride(const QString &name, std::optional<bool> enabled)
{
    auto *s = Core::ICore::settings();
    if (!s)
        return;
    if (!enabled.has_value())
        s->remove(toolsOverrideKey(name));
    else
        s->setValue(toolsOverrideKey(name), *enabled);
    s->sync();
}
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
    return Core::ICore::userResourcePath(QStringLiteral("qodeassist/config/agents")).toFSPathString();
}

QString AgentFactory::userConfigDir()
{
    return Core::ICore::userResourcePath(QStringLiteral("qodeassist")).toFSPathString();
}

void AgentFactory::reload()
{
    Q_ASSERT(thread() == QThread::currentThread());
    clear();

    QDir().mkpath(userAgentsDir());
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
    for (auto &cfg : m_configs) {
        m_baseModelByName.insert(cfg.name, cfg.model);
        const QString overrideModel = readModelOverride(cfg.name);
        if (!overrideModel.isEmpty())
            cfg.model = overrideModel;

        m_baseProviderByName.insert(cfg.name, cfg.providerInstance);
        const QString overrideProvider = readProviderOverride(cfg.name);
        if (!overrideProvider.isEmpty())
            cfg.providerInstance = overrideProvider;

        m_baseToolsByName.insert(cfg.name, cfg.enableTools);
        const std::optional<bool> overrideTools = readToolsOverride(cfg.name);
        if (overrideTools.has_value())
            cfg.enableTools = *overrideTools;
    }
    emit agentsChanged();
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
        if (c.hidden)
            continue;
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
    const Providers::ProviderInstance *inst = instanceFactory->instanceByName(cfg.providerInstance);
    if (!inst) {
        if (errorOut) {
            *errorOut = QStringLiteral("Agent '%1' references unknown provider instance '%2'")
                            .arg(cfg.name, cfg.providerInstance);
        }
        return nullptr;
    }
    const QString validation
        = Providers::ProviderInstance::validate(*inst, Providers::ProviderFactory::knownNames());
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
    Providers::Provider *provider
        = buildProviderForAgent(*cfg, m_instanceFactory.data(), m_secrets.data(), errorOut);
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

Agent *AgentFactory::createFromFile(const QString &tomlPath, QObject *parent, QString *errorOut) const
{
    QString parseErr;
    QStringList warnings;
    auto cfgOpt = Agents::AgentLoader::parseFile(tomlPath, agentQrcPrefix(), &parseErr, &warnings);
    if (!cfgOpt) {
        if (errorOut)
            *errorOut = parseErr;
        return nullptr;
    }
    Providers::Provider *provider
        = buildProviderForAgent(*cfgOpt, m_instanceFactory.data(), m_secrets.data(), errorOut);
    if (!provider)
        return nullptr;
    auto agent = std::make_unique<Agent>(std::move(*cfgOpt), provider, /*parent=*/nullptr);
    if (!agent->isValid()) {
        if (errorOut)
            *errorOut = agent->invalidReason();
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
    m_baseModelByName.clear();
    m_baseProviderByName.clear();
    m_baseToolsByName.clear();
}

Providers::ProviderInstanceFactory *AgentFactory::instanceFactory() const noexcept
{
    return m_instanceFactory.data();
}

bool AgentFactory::setAgentModelOverride(const QString &name, const QString &model, QString *error)
{
    Q_ASSERT(thread() == QThread::currentThread());

    const auto it = m_indexByName.constFind(name);
    if (it == m_indexByName.constEnd()) {
        if (error)
            *error = QStringLiteral("Agent '%1' is not registered").arg(name);
        return false;
    }
    AgentConfig &cfg = m_configs[it.value()];
    const QString effective = model.isEmpty() ? m_baseModelByName.value(name, cfg.model) : model;
    if (cfg.model == effective && readModelOverride(name) == model)
        return true;

    writeModelOverride(name, model);
    cfg.model = effective;
    emit agentModelChanged(name);
    return true;
}

QString AgentFactory::agentModelOverride(const QString &name) const
{
    return readModelOverride(name);
}

void AgentFactory::clearAgentModelOverride(const QString &name)
{
    writeModelOverride(name, QString());
}

bool AgentFactory::setAgentProviderOverride(
    const QString &name, const QString &providerInstance, QString *error)
{
    Q_ASSERT(thread() == QThread::currentThread());

    const auto it = m_indexByName.constFind(name);
    if (it == m_indexByName.constEnd()) {
        if (error)
            *error = QStringLiteral("Agent '%1' is not registered").arg(name);
        return false;
    }
    AgentConfig &cfg = m_configs[it.value()];
    const QString effective = providerInstance.isEmpty()
                                  ? m_baseProviderByName.value(name, cfg.providerInstance)
                                  : providerInstance;
    if (cfg.providerInstance == effective && readProviderOverride(name) == providerInstance)
        return true;

    writeProviderOverride(name, providerInstance);
    cfg.providerInstance = effective;
    emit agentProviderChanged(name);
    return true;
}

QString AgentFactory::agentProviderOverride(const QString &name) const
{
    return readProviderOverride(name);
}

void AgentFactory::clearAgentProviderOverride(const QString &name)
{
    writeProviderOverride(name, QString());
}

bool AgentFactory::setAgentToolsOverride(
    const QString &name, std::optional<bool> enabled, QString *error)
{
    Q_ASSERT(thread() == QThread::currentThread());

    const auto it = m_indexByName.constFind(name);
    if (it == m_indexByName.constEnd()) {
        if (error)
            *error = QStringLiteral("Agent '%1' is not registered").arg(name);
        return false;
    }
    AgentConfig &cfg = m_configs[it.value()];
    const bool effective = enabled.has_value() ? *enabled
                                               : m_baseToolsByName.value(name, cfg.enableTools);
    if (cfg.enableTools == effective && readToolsOverride(name) == enabled)
        return true;

    writeToolsOverride(name, enabled);
    cfg.enableTools = effective;
    emit agentToolsChanged(name);
    return true;
}

std::optional<bool> AgentFactory::agentToolsOverride(const QString &name) const
{
    return readToolsOverride(name);
}

void AgentFactory::clearAgentToolsOverride(const QString &name)
{
    writeToolsOverride(name, std::nullopt);
}

} // namespace QodeAssist
