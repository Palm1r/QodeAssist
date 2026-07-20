// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentCatalogStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

#include <coreplugin/icore.h>

#include <logger/Logger.hpp>

#include "acp/AgentRegistryParser.hpp"

namespace QodeAssist::Acp {

namespace {

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return file.readAll();
}

} // namespace

AgentCatalogStore::AgentCatalogStore(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    reload();
}

QString AgentCatalogStore::userAgentsDirectory()
{
    const QString path = QStringLiteral("%1/qodeassist/agents")
                             .arg(Core::ICore::userResourcePath().toFSPathString());
    QDir().mkpath(path);
    return path;
}

QString AgentCatalogStore::registryCachePath()
{
    const QString directory
        = QStringLiteral("%1/qodeassist").arg(Core::ICore::userResourcePath().toFSPathString());
    QDir().mkpath(directory);
    return directory + QStringLiteral("/acp-registry-cache.json");
}

QUrl AgentCatalogStore::registryUrl()
{
    return QUrl(
        QStringLiteral("https://cdn.agentclientprotocol.com/registry/v1/latest/registry.json"));
}

QString AgentCatalogStore::bundledSnapshotPath()
{
    return QStringLiteral(":/resources/agents/acp-registry-snapshot.json");
}

bool AgentCatalogStore::hasCachedRegistry() const
{
    return QFileInfo::exists(registryCachePath());
}

void AgentCatalogStore::reload()
{
    m_warnings.clear();
    loadBundledSnapshot();
    loadRegistryCache();
    loadUserFiles();
    emit catalogChanged();
}

void AgentCatalogStore::loadBundledSnapshot()
{
    const AgentParseResult result = AgentRegistryParser::parse(
        readFile(bundledSnapshotPath()),
        AgentSource::BundledSnapshot,
        QStringLiteral("bundled snapshot"));

    m_warnings.append(result.warnings);
    m_catalog.setLayer(AgentSource::BundledSnapshot, result.agents);
}

void AgentCatalogStore::loadRegistryCache()
{
    const QString path = registryCachePath();
    if (!QFileInfo::exists(path)) {
        m_catalog.setLayer(AgentSource::LiveRegistry, {});
        return;
    }

    const AgentParseResult result = AgentRegistryParser::parse(
        readFile(path), AgentSource::LiveRegistry, QStringLiteral("registry cache"));

    m_warnings.append(result.warnings);
    m_catalog.setLayer(AgentSource::LiveRegistry, result.agents);
}

void AgentCatalogStore::loadUserFiles()
{
    const QDir directory(userAgentsDirectory());
    const QStringList files
        = directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);

    QList<AgentDefinition> agents;
    for (const QString &file : files) {
        const QString path = directory.filePath(file);
        const AgentParseResult result
            = AgentRegistryParser::parse(readFile(path), AgentSource::UserFile, file);
        m_warnings.append(result.warnings);
        agents.append(result.agents);
    }

    m_catalog.setLayer(AgentSource::UserFile, agents);
}

void AgentCatalogStore::refreshFromRegistry()
{
    if (m_reply)
        return;

    m_reply = m_network->get(QNetworkRequest(registryUrl()));
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit refreshFinished(false, reply->errorString());
            return;
        }

        const QByteArray payload = reply->readAll();
        const AgentParseResult result = AgentRegistryParser::parse(
            payload, AgentSource::LiveRegistry, QStringLiteral("registry"));

        if (result.agents.isEmpty()) {
            emit refreshFinished(false, tr("The registry response contained no agents."));
            return;
        }

        QSaveFile cache(registryCachePath());
        if (!cache.open(QIODevice::WriteOnly) || cache.write(payload) != payload.size()
            || !cache.commit()) {
            emit refreshFinished(
                false, tr("Cannot write the registry cache: %1").arg(cache.errorString()));
            return;
        }

        LOG_MESSAGE(QString("ACP registry refreshed: %1 agents").arg(result.agents.size()));

        reload();
        emit refreshFinished(true, {});
    });
}

} // namespace QodeAssist::Acp
