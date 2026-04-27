// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "McpClientsManager.hpp"

#include <coreplugin/icore.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

#include <logger/Logger.hpp>
#include <pluginllmcore/Provider.hpp>
#include <pluginllmcore/ProvidersManager.hpp>
#include <settings/McpSettings.hpp>

namespace QodeAssist::Mcp {

namespace {

constexpr const char *kServersKey = "mcpServers";

QByteArray serializedEntry(const McpServerConfig &cfg)
{
    return QJsonDocument(cfg.toJson()).toJson(QJsonDocument::Compact);
}

} // namespace

McpClientsManager &McpClientsManager::instance()
{
    static McpClientsManager manager;
    return manager;
}

QString McpClientsManager::configFilePath()
{
    const QString base = Core::ICore::userResourcePath().toFSPathString();
    return QDir(base).filePath(QStringLiteral("qodeassist/mcp-server.json"));
}

QByteArray McpClientsManager::emptyConfigTemplate()
{
    return QByteArray(
        "{\n"
        "  \"mcpServers\": {\n"
        "    // Example HTTP/SSE server:\n"
        "    // \"my-http-server\": {\n"
        "    //   \"type\": \"sse\",\n"
        "    //   \"url\": \"http://127.0.0.1:9000/mcp\",\n"
        "    //   \"enable\": true\n"
        "    // },\n"
        "    // Example stdio server:\n"
        "    // \"my-stdio-server\": {\n"
        "    //   \"command\": \"/path/to/server\",\n"
        "    //   \"args\": [\"--flag\"],\n"
        "    //   \"env\": {\"KEY\": \"value\"},\n"
        "    //   \"enable\": false\n"
        "    // }\n"
        "  }\n"
        "}\n");
}

McpClientsManager::McpClientsManager(QObject *parent)
    : QObject(parent)
{}

McpClientsManager::~McpClientsManager()
{
    for (auto *c : m_connections) {
        if (c)
            c->disconnectFromServer();
    }
    m_connections.clear();
}

void McpClientsManager::init()
{
    if (m_initialized)
        return;
    m_initialized = true;

    ensureFileExists();
    setupWatcher();

    connect(
        &Settings::mcpSettings().enableMcpClients,
        &Utils::BaseAspect::changed,
        this,
        [this]() { loadFromDisk(); });

    connect(
        &Settings::mcpSettings().mcpClientExtraPaths,
        &Utils::BaseAspect::changed,
        this,
        [this]() {
            for (auto *c : m_connections) {
                if (c && c->config().transport == McpTransportKind::Stdio
                    && c->config().enabled
                    && Settings::mcpSettings().enableMcpClients()) {
                    c->disconnectFromServer();
                    c->connectToServer();
                }
            }
        });

    loadFromDisk();
}

void McpClientsManager::ensureFileExists()
{
    const QString path = configFilePath();
    QFileInfo fi(path);
    if (fi.exists())
        return;

    QDir().mkpath(fi.absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(emptyConfigTemplate());
        f.close();
        LOG_MESSAGE(QString("Created empty MCP clients config: %1").arg(path));
    }
}

void McpClientsManager::setupWatcher()
{
    m_watcher = new QFileSystemWatcher(this);
    m_reloadDebounce = new QTimer(this);
    m_reloadDebounce->setSingleShot(true);
    m_reloadDebounce->setInterval(300);

    connect(m_reloadDebounce.data(), &QTimer::timeout, this, [this]() {
        const bool suppress = m_suppressNextWatcherReload;
        m_suppressNextWatcherReload = false;
        if (!suppress)
            loadFromDisk();
        updateWatchedPaths();
    });
    connect(m_watcher.data(), &QFileSystemWatcher::fileChanged, this, [this]() {
        m_reloadDebounce->start();
    });
    connect(m_watcher.data(), &QFileSystemWatcher::directoryChanged, this, [this]() {
        m_reloadDebounce->start();
    });

    updateWatchedPaths();
}

void McpClientsManager::updateWatchedPaths()
{
    if (!m_watcher)
        return;
    if (!m_watcher->files().isEmpty())
        m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());

    const QString path = configFilePath();
    const QFileInfo info(path);
    if (info.exists())
        m_watcher->addPath(path);
    const QString dir = info.absolutePath();
    if (QFileInfo::exists(dir))
        m_watcher->addPath(dir);
}

QList<McpServerConnection *> McpClientsManager::connections() const
{
    return m_connections;
}

QList<PluginLLMCore::Provider *> McpClientsManager::toolsCapableProviders() const
{
    QList<PluginLLMCore::Provider *> out;
    auto &pm = PluginLLMCore::ProvidersManager::instance();
    for (const QString &name : pm.providersNames()) {
        auto *p = pm.getProviderByName(name);
        if (!p)
            continue;
        if (p->capabilities().testFlag(PluginLLMCore::ProviderCapability::Tools))
            out.append(p);
    }
    return out;
}

QJsonObject McpClientsManager::readRoot() const
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QJsonObject{{QLatin1String(kServersKey), QJsonObject{}}};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject{{QLatin1String(kServersKey), QJsonObject{}}};
    QJsonObject root = doc.object();
    if (!root.contains(QLatin1String(kServersKey)))
        root.insert(QLatin1String(kServersKey), QJsonObject{});
    return root;
}

bool McpClientsManager::writeRoot(const QJsonObject &root)
{
    QSaveFile out(configFilePath());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const QString reason = out.errorString();
        LOG_MESSAGE(
            QString("MCP clients: cannot write %1: %2").arg(configFilePath(), reason));
        emit writeFailed(reason);
        return false;
    }
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!out.commit()) {
        const QString reason = out.errorString();
        LOG_MESSAGE(
            QString("MCP clients: commit failed for %1: %2").arg(configFilePath(), reason));
        emit writeFailed(reason);
        return false;
    }
    m_suppressNextWatcherReload = true;
    return true;
}

void McpClientsManager::reload()
{
    loadFromDisk();
    updateWatchedPaths();
}

bool McpClientsManager::setServerEnabled(const QString &name, bool enabled)
{
    QJsonObject root = readRoot();
    QJsonObject servers = root.value(QLatin1String(kServersKey)).toObject();
    if (!servers.contains(name)) {
        LOG_MESSAGE(QString("MCP clients: setServerEnabled: no entry '%1'").arg(name));
        return false;
    }
    QJsonObject entry = servers.value(name).toObject();
    entry.insert(QStringLiteral("enable"), enabled);
    servers.insert(name, entry);
    root.insert(QLatin1String(kServersKey), servers);
    if (!writeRoot(root))
        return false;
    loadFromDisk();
    return true;
}

bool McpClientsManager::addServer(const QString &name, const QJsonObject &entry)
{
    QJsonObject root = readRoot();
    QJsonObject servers = root.value(QLatin1String(kServersKey)).toObject();

    QString finalName = name;
    int suffix = 2;
    while (servers.contains(finalName))
        finalName = QStringLiteral("%1-%2").arg(name).arg(suffix++);

    servers.insert(finalName, entry);
    root.insert(QLatin1String(kServersKey), servers);
    if (!writeRoot(root))
        return false;
    loadFromDisk();
    return true;
}

bool McpClientsManager::removeServer(const QString &name)
{
    QJsonObject root = readRoot();
    QJsonObject servers = root.value(QLatin1String(kServersKey)).toObject();
    if (!servers.contains(name))
        return false;
    servers.remove(name);
    root.insert(QLatin1String(kServersKey), servers);
    if (!writeRoot(root))
        return false;
    loadFromDisk();
    return true;
}

void McpClientsManager::loadFromDisk()
{
    const QJsonObject root = readRoot();
    const QJsonObject servers = root.value(QLatin1String(kServersKey)).toObject();

    QList<McpServerConfig> newConfigs;
    for (auto it = servers.begin(); it != servers.end(); ++it) {
        if (!it.value().isObject())
            continue;
        newConfigs.append(McpServerConfig::fromJson(it.key(), it.value().toObject()));
    }

    const auto providers = toolsCapableProviders();

    const bool masterEnabled = Settings::mcpSettings().enableMcpClients();

    QList<McpServerConnection *> keep;
    QList<McpServerConnection *> oldConnections = m_connections;
    m_connections.clear();

    for (const McpServerConfig &cfg : newConfigs) {
        McpServerConnection *existing = nullptr;
        for (auto *c : oldConnections) {
            if (c && c->config().name == cfg.name) {
                existing = c;
                break;
            }
        }

        const bool configUnchanged
            = existing && serializedEntry(existing->config()) == serializedEntry(cfg);

        McpServerConnection *c = nullptr;
        if (configUnchanged) {
            oldConnections.removeAll(existing);
            c = existing;
        } else {
            if (existing) {
                oldConnections.removeAll(existing);
                existing->disconnectFromServer();
                existing->deleteLater();
            }
            c = new McpServerConnection(cfg, this);
            c->setProviders(providers);
            connect(
                c,
                &McpServerConnection::stateChanged,
                this,
                &McpClientsManager::serversChanged);
        }
        keep.append(c);

        const bool wantRunning = cfg.enabled && masterEnabled;
        const bool currentlyRunning = c->state() == McpConnectionState::Connected
                                      || c->state() == McpConnectionState::Connecting;

        if (wantRunning && !currentlyRunning)
            c->connectToServer();
        else if (!wantRunning && currentlyRunning)
            c->disconnectFromServer();
    }

    for (auto *c : oldConnections) {
        if (!c)
            continue;
        c->disconnectFromServer();
        c->deleteLater();
    }

    m_connections = keep;

    LOG_MESSAGE(QString("MCP clients: loaded %1 server(s) from %2")
                    .arg(m_connections.size())
                    .arg(configFilePath()));

    emit serversChanged();
}

} // namespace QodeAssist::Mcp
