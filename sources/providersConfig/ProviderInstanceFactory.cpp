// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProviderInstanceFactory.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QSet>
#include <QThread>
#include <QTimer>

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

    m_watcher = new QFileSystemWatcher(this);
    m_reloadDebounce = new QTimer(this);
    m_reloadDebounce->setSingleShot(true);
    m_reloadDebounce->setInterval(150);
    connect(m_reloadDebounce, &QTimer::timeout, this, [this] { reload(); });
    auto kick = [this](const QString &) { m_reloadDebounce->start(); };
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, kick);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, kick);

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
    rewatchUserDir();
    emit instancesReloaded();
}

void ProviderInstanceFactory::rebuildIndexes()
{
    m_nameIndex.clear();
    m_instanceNamesCache.clear();
    m_knownClientApisCache.clear();
    m_nameIndex.reserve(static_cast<qsizetype>(m_instances.size()));
    m_instanceNamesCache.reserve(static_cast<qsizetype>(m_instances.size()));

    std::sort(m_instances.begin(), m_instances.end(),
              [](const ProviderInstance &a, const ProviderInstance &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });

    QSet<QString> seenApis;
    for (qsizetype i = 0; i < static_cast<qsizetype>(m_instances.size()); ++i) {
        const ProviderInstance &inst = m_instances[i];
        m_nameIndex.insert(inst.name.toCaseFolded(), i);
        m_instanceNamesCache.append(inst.name);
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

void ProviderInstanceFactory::rewatchUserDir()
{
    if (!m_watcher)
        return;

    const QStringList stale = m_watcher->files() + m_watcher->directories();
    if (!stale.isEmpty())
        m_watcher->removePaths(stale);

    const QString userDir = userInstancesDir();
    QDir().mkpath(userDir);
    m_watcher->addPath(userDir);
    QDir d(userDir);
    for (const QFileInfo &fi : d.entryInfoList({"*.toml"}, QDir::Files))
        m_watcher->addPath(fi.absoluteFilePath());
}

void ProviderInstanceFactory::registerInstance(ProviderInstance instance)
{
    Q_ASSERT_X(QThread::currentThread() == thread(),
               Q_FUNC_INFO, "ProviderInstanceFactory must be used from its owner thread");
    const QString validation = ProviderInstance::validate(instance, knownClientApis());
    if (!validation.isEmpty()) {
        qCWarning(providerInstanceFactoryLog).noquote()
            << "Refusing to register provider instance:" << validation;
        return;
    }
    const QString name = instance.name;
    for (auto &existing : m_instances) {
        if (existing.name == name) {
            existing = std::move(instance);
            emit instanceChanged(name);
            return;
        }
    }
    m_instances.push_back(std::move(instance));
    rebuildIndexes();
    emit instanceChanged(name);
}

const ProviderInstance *ProviderInstanceFactory::instanceByName(const QString &name) const
{
    const auto it = m_nameIndex.constFind(name.toCaseFolded());
    if (it == m_nameIndex.constEnd())
        return nullptr;
    return &m_instances[it.value()];
}

QStringList ProviderInstanceFactory::instanceNames() const
{
    return m_instanceNamesCache;
}

QStringList ProviderInstanceFactory::instanceNamesForClientApi(const QString &clientApi) const
{
    QStringList out;
    for (const auto &inst : m_instances) {
        if (inst.clientApi == clientApi)
            out.append(inst.name);
    }
    return out;
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
    m_instanceNamesCache.clear();
    m_knownClientApisCache.clear();
    m_errors.clear();
    m_warnings.clear();
}

} // namespace QodeAssist::Providers
