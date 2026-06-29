// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ProviderInstanceFactory.hpp"

#include <QDir>
#include <QLoggingCategory>
#include <QSet>
#include <QThread>

#include <coreplugin/icore.h>

#include <algorithm>

#include "ProviderInstanceLoader.hpp"
#include "Logger.hpp"

static inline void initProviderInstancesResource()
{
    Q_INIT_RESOURCE(provider_instances);
}

namespace {

Q_LOGGING_CATEGORY(providerInstanceFactoryLog, "qodeassist.providerinstancefactory")

QString instanceQrcPrefix() { return QStringLiteral(":/provider-instances"); }

} // namespace

namespace QodeAssist::Providers {

ProviderInstanceFactory::ProviderInstanceFactory(QObject *parent)
    : QObject(parent)
{
    ::initProviderInstancesResource();

    reload();
}

ProviderInstanceFactory::~ProviderInstanceFactory() = default;

QString ProviderInstanceFactory::userInstancesDir()
{
    return Core::ICore::userResourcePath(
               QStringLiteral("qodeassist/config/providers")).toFSPathString();
}

void ProviderInstanceFactory::reload()
{
    Q_ASSERT_X(QThread::currentThread() == thread(),
               Q_FUNC_INFO, "ProviderInstanceFactory must be used from its owner thread");
    clear();

    QDir().mkpath(userInstancesDir());
    auto result = ProviderInstanceLoader::load(instanceQrcPrefix(), userInstancesDir());
    for (const QString &err : result.errors)
        LOG_MESSAGE(QString("[ProviderInstances] error: %1").arg(err));
    for (const QString &warn : result.warnings)
        LOG_MESSAGE(QString("[ProviderInstances] warning: %1").arg(warn));
    LOG_MESSAGE(QString("[ProviderInstances] Loaded %1 instances (qrc=%2, user=%3)")
                    .arg(result.instances.size())
                    .arg(instanceQrcPrefix(), userInstancesDir()));

    for (auto &inst : result.instances) {
        LOG_MESSAGE(QString("[ProviderInstances] Loaded: %1 (client_api=%2, url=%3)")
                        .arg(inst.name, inst.clientApi, inst.url));
        m_instances.push_back(std::move(inst));
    }
    m_errors = std::move(result.errors);
    m_warnings = std::move(result.warnings);

    rebuildIndexes();
    emit instancesReloaded();
}

void ProviderInstanceFactory::rebuildIndexes()
{
    m_nameIndex.clear();
    m_knownClientApisCache.clear();
    m_nameIndex.reserve(static_cast<qsizetype>(m_instances.size()));

    std::sort(m_instances.begin(), m_instances.end(),
              [](const ProviderInstance &a, const ProviderInstance &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });

    QSet<QString> seenApis;
    for (qsizetype i = 0; i < static_cast<qsizetype>(m_instances.size()); ++i) {
        const ProviderInstance &inst = m_instances[i];
        m_nameIndex.insert(inst.name.toCaseFolded(), i);
        if (!seenApis.contains(inst.clientApi)) {
            seenApis.insert(inst.clientApi);
            m_knownClientApisCache.append(inst.clientApi);
        }
    }
    std::sort(m_knownClientApisCache.begin(), m_knownClientApisCache.end(),
              [](const QString &a, const QString &b) {
                  return a.compare(b, Qt::CaseInsensitive) < 0;
              });
}

const ProviderInstance *ProviderInstanceFactory::instanceByName(const QString &name) const
{
    const auto it = m_nameIndex.constFind(name.toCaseFolded());
    if (it == m_nameIndex.constEnd())
        return nullptr;
    return &m_instances[it.value()];
}

QStringList ProviderInstanceFactory::knownClientApis() const
{
    return m_knownClientApisCache;
}

void ProviderInstanceFactory::clear()
{
    Q_ASSERT_X(QThread::currentThread() == thread(),
               Q_FUNC_INFO, "ProviderInstanceFactory must be used from its owner thread");
    m_instances.clear();
    m_nameIndex.clear();
    m_knownClientApisCache.clear();
    m_errors.clear();
    m_warnings.clear();
}

} // namespace QodeAssist::Providers
