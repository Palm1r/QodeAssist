// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "McpServerConnection.hpp"

#include <LLMQore/McpClient.hpp>
#include <LLMQore/McpExceptions.hpp>
#include <LLMQore/McpHttpTransport.hpp>
#include <LLMQore/McpRemoteTool.hpp>
#include <LLMQore/McpStdioTransport.hpp>
#include <LLMQore/McpTransport.hpp>
#include <LLMQore/McpTypes.hpp>
#include <LLMQore/ToolsManager.hpp>
#include <LLMQore/Version.hpp>

#include <QDir>
#include <QFileInfo>
#include <QFuture>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>

#include <logger/Logger.hpp>
#include <pluginllmcore/Provider.hpp>
#include <settings/McpSettings.hpp>

namespace QodeAssist::Mcp {

namespace {

QString transportToString(McpTransportKind k)
{
    return k == McpTransportKind::Http ? QStringLiteral("http") : QStringLiteral("stdio");
}

bool providerSupportsTools(PluginLLMCore::Provider *p)
{
    if (!p)
        return false;
    return p->capabilities().testFlag(PluginLLMCore::ProviderCapability::Tools);
}

} // namespace

McpServerConfig McpServerConfig::fromJson(const QString &name, const QJsonObject &obj)
{
    McpServerConfig cfg;
    cfg.name = name;
    // Accept both "enable" (preferred) and "enabled" (legacy).
    if (obj.contains(QStringLiteral("enable")))
        cfg.enabled = obj.value(QStringLiteral("enable")).toBool(true);
    else
        cfg.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

    const QString type = obj.value(QStringLiteral("type")).toString().toLower();
    const QString transport = obj.value(QStringLiteral("transport")).toString().toLower();
    const bool hasCommand = obj.contains(QStringLiteral("command"));
    const bool hasUrl = obj.contains(QStringLiteral("url"));

    if (transport == QStringLiteral("stdio") || type == QStringLiteral("stdio")
        || (hasCommand && !hasUrl)) {
        cfg.transport = McpTransportKind::Stdio;
    } else {
        cfg.transport = McpTransportKind::Http;
    }

    if (cfg.transport == McpTransportKind::Http) {
        cfg.url = QUrl(obj.value(QStringLiteral("url")).toString());
        cfg.spec = obj.value(QStringLiteral("spec")).toString();
        const QJsonObject headers = obj.value(QStringLiteral("headers")).toObject();
        for (auto it = headers.begin(); it != headers.end(); ++it)
            cfg.headers.insert(it.key(), it.value().toString());
    } else {
        cfg.command = obj.value(QStringLiteral("command")).toString();
        const QJsonArray args = obj.value(QStringLiteral("args")).toArray();
        for (const QJsonValue &v : args)
            cfg.args.append(v.toString());
        const QJsonObject env = obj.value(QStringLiteral("env")).toObject();
        for (auto it = env.begin(); it != env.end(); ++it)
            cfg.env.insert(it.key(), it.value().toString());
        cfg.workingDirectory = obj.value(QStringLiteral("cwd")).toString();
    }

    return cfg;
}

QJsonObject McpServerConfig::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("enable")] = enabled;

    if (transport == McpTransportKind::Http) {
        obj[QStringLiteral("type")] = QStringLiteral("sse");
        obj[QStringLiteral("url")] = url.toString();
        if (!spec.isEmpty())
            obj[QStringLiteral("spec")] = spec;
        if (!headers.isEmpty()) {
            QJsonObject h;
            for (auto it = headers.begin(); it != headers.end(); ++it)
                h[it.key()] = it.value();
            obj[QStringLiteral("headers")] = h;
        }
    } else {
        obj[QStringLiteral("type")] = QStringLiteral("stdio");
        obj[QStringLiteral("command")] = command;
        if (!args.isEmpty()) {
            QJsonArray a;
            for (const QString &s : args)
                a.append(s);
            obj[QStringLiteral("args")] = a;
        }
        if (!env.isEmpty()) {
            QJsonObject e;
            for (auto it = env.begin(); it != env.end(); ++it)
                e[it.key()] = it.value();
            obj[QStringLiteral("env")] = e;
        }
        if (!workingDirectory.isEmpty())
            obj[QStringLiteral("cwd")] = workingDirectory;
    }
    return obj;
}

McpServerConnection::McpServerConnection(McpServerConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
{}

McpServerConnection::~McpServerConnection()
{
    disconnectFromServer();
}

void McpServerConnection::setProviders(const QList<PluginLLMCore::Provider *> &providers)
{
    m_providers.clear();
    for (auto *p : providers) {
        if (providerSupportsTools(p))
            m_providers.append(p);
    }
}

::LLMQore::Mcp::McpTransport *McpServerConnection::createTransport()
{
    if (m_config.transport == McpTransportKind::Http) {
        if (!m_config.url.isValid()) {
            setState(
                McpConnectionState::Failed,
                QStringLiteral("Invalid URL: %1").arg(m_config.url.toString()));
            return nullptr;
        }
        ::LLMQore::Mcp::HttpTransportConfig cfg;
        cfg.endpoint = m_config.url;
        cfg.headers = m_config.headers;
        if (m_config.spec == QLatin1String("2024-11-05"))
            cfg.spec = ::LLMQore::Mcp::McpHttpSpec::V2024_11_05;
        else if (m_config.spec == QLatin1String("2025-03-26"))
            cfg.spec = ::LLMQore::Mcp::McpHttpSpec::V2025_03_26;
        return new ::LLMQore::Mcp::McpHttpTransport(cfg, nullptr, this);
    }

    if (m_config.command.isEmpty()) {
        setState(McpConnectionState::Failed, QStringLiteral("Empty command"));
        return nullptr;
    }

    ::LLMQore::Mcp::StdioLaunchConfig cfg;
    cfg.arguments = m_config.args;
    cfg.workingDirectory = m_config.workingDirectory;

    QStringList extraDirs;
    const QString extraPaths
        = Settings::mcpSettings().mcpClientExtraPaths.volatileValue().trimmed();
    if (!extraPaths.isEmpty()) {
        const QChar sep = QDir::listSeparator();
        for (const QString &p : extraPaths.split(sep, Qt::SkipEmptyParts)) {
            const QString t = p.trimmed();
            if (!t.isEmpty())
                extraDirs << t;
        }
    }

    // QProcess looks up a bare program name against the *parent* PATH before
    // the custom environment below is applied to the child. Resolve the
    // absolute path ourselves using the augmented search dirs so the user's
    // "Extra PATH" field actually matters.
    QString resolvedProgram = m_config.command;
    const bool isBareName = !resolvedProgram.isEmpty()
                            && !resolvedProgram.contains(QLatin1Char('/'))
                            && !resolvedProgram.contains(QLatin1Char('\\'));
    if (isBareName) {
        const QString found = QStandardPaths::findExecutable(resolvedProgram, extraDirs);
        if (!found.isEmpty())
            resolvedProgram = found;
    }
    cfg.program = resolvedProgram;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!extraDirs.isEmpty()) {
        const QChar sep = QDir::listSeparator();
        const QString existing = env.value(QStringLiteral("PATH"));
        const QString merged = existing.isEmpty()
                                   ? extraDirs.join(sep)
                                   : extraDirs.join(sep) + sep + existing;
        env.insert(QStringLiteral("PATH"), merged);
    }

    for (auto it = m_config.env.begin(); it != m_config.env.end(); ++it)
        env.insert(it.key(), it.value());
    cfg.environment = env;

    return new ::LLMQore::Mcp::McpStdioClientTransport(cfg, this);
}

void McpServerConnection::connectToServer()
{
    if (m_state == McpConnectionState::Connecting || m_state == McpConnectionState::Connected)
        return;

    setState(McpConnectionState::Connecting, QStringLiteral("Connecting..."));

    m_transport = createTransport();
    if (!m_transport)
        return;

    ::LLMQore::Mcp::Implementation clientInfo;
    clientInfo.name = QStringLiteral("QodeAssist");
    clientInfo.version = QStringLiteral(LLMQORE_VERSION_STRING);

    m_client = new ::LLMQore::Mcp::McpClient(m_transport.data(), clientInfo, this);

    connect(m_client.data(), &::LLMQore::Mcp::McpClient::toolsChanged, this, [this]() {
        unregisterTools();
        fetchAndRegisterTools();
    });

    connect(m_client.data(), &::LLMQore::Mcp::McpClient::disconnected, this, [this]() {
        setState(McpConnectionState::Failed, QStringLiteral("Disconnected"));
        unregisterTools();
    });

    connect(
        m_client.data(),
        &::LLMQore::Mcp::McpClient::errorOccurred,
        this,
        [this](const QString &error) {
            LOG_MESSAGE(
                QString("MCP client [%1] error: %2").arg(m_config.name, error));
        });

    m_client->connectAndInitialize(std::chrono::seconds(15))
        .then(this,
              [this](const ::LLMQore::Mcp::InitializeResult &result) {
                  LOG_MESSAGE(QString("MCP client [%1] connected to %2 %3")
                                  .arg(m_config.name,
                                       result.serverInfo.name,
                                       result.serverInfo.version));
                  fetchAndRegisterTools();
              })
        .onFailed(this, [this](const std::exception &e) {
            const QString msg = QString::fromUtf8(e.what());
            LOG_MESSAGE(QString("MCP client [%1] init failed: %2").arg(m_config.name, msg));
            setState(McpConnectionState::Failed, msg);
            disconnectFromServer();
        });
}

void McpServerConnection::fetchAndRegisterTools()
{
    if (!m_client)
        return;

    if (m_listToolsWatchdog) {
        m_listToolsWatchdog->stop();
        m_listToolsWatchdog->deleteLater();
        m_listToolsWatchdog.clear();
    }
    m_listToolsWatchdog = new QTimer(this);
    m_listToolsWatchdog->setSingleShot(true);
    connect(m_listToolsWatchdog.data(), &QTimer::timeout, this, [this]() {
        if (m_state == McpConnectionState::Connected)
            return;
        LOG_MESSAGE(QString("MCP client [%1] listTools timed out").arg(m_config.name));
        setState(McpConnectionState::Failed, QStringLiteral("listTools timed out"));
        disconnectFromServer();
    });
    m_listToolsWatchdog->start(std::chrono::seconds(15));

    m_client->listTools()
        .then(this,
              [this](const QList<::LLMQore::Mcp::ToolInfo> &tools) {
                  if (m_listToolsWatchdog)
                      m_listToolsWatchdog->stop();
                  if (m_providers.isEmpty()) {
                      LOG_MESSAGE(QString("MCP client [%1]: no tools-capable providers to "
                                          "register %2 tools into")
                                      .arg(m_config.name)
                                      .arg(tools.size()));
                      setState(
                          McpConnectionState::Connected,
                          QStringLiteral("Connected (%1 tools)").arg(tools.size()));
                      return;
                  }

                  for (const auto &info : tools) {
                      if (info.name.isEmpty())
                          continue;
                      m_toolIds.append(info.name);
                      for (const auto &p : m_providers) {
                          if (!p)
                              continue;
                          auto *tm = p->toolsManager();
                          if (!tm)
                              continue;
                          auto *remote = new ::LLMQore::Mcp::McpRemoteTool(
                              m_client.data(), info, tm);
                          tm->addTool(remote);
                      }
                  }

                  LOG_MESSAGE(QString("MCP client [%1]: registered %2 tools across %3 providers")
                                  .arg(m_config.name)
                                  .arg(tools.size())
                                  .arg(m_providers.size()));
                  setState(
                      McpConnectionState::Connected,
                      QStringLiteral("Connected (%1 tools)").arg(tools.size()));
              })
        .onFailed(this, [this](const std::exception &e) {
            if (m_listToolsWatchdog)
                m_listToolsWatchdog->stop();
            const QString msg = QString::fromUtf8(e.what());
            LOG_MESSAGE(QString("MCP client [%1] listTools failed: %2").arg(m_config.name, msg));
            setState(McpConnectionState::Failed, msg);
        });
}

void McpServerConnection::unregisterTools()
{
    if (m_toolIds.isEmpty())
        return;

    for (const auto &p : m_providers) {
        if (!p)
            continue;
        auto *tm = p->toolsManager();
        if (!tm)
            continue;
        for (const QString &id : m_toolIds)
            tm->removeTool(id);
    }
    m_toolIds.clear();
}

void McpServerConnection::disconnectFromServer()
{
    if (m_listToolsWatchdog) {
        m_listToolsWatchdog->stop();
        m_listToolsWatchdog->deleteLater();
        m_listToolsWatchdog.clear();
    }

    unregisterTools();

    if (m_client) {
        m_client->shutdown();
        m_client->deleteLater();
        m_client.clear();
    }
    if (m_transport) {
        m_transport->deleteLater();
        m_transport.clear();
    }

    if (m_state != McpConnectionState::Failed)
        setState(McpConnectionState::Disabled, QStringLiteral("Disabled"));
}

void McpServerConnection::setState(McpConnectionState state, const QString &text)
{
    if (m_state == state && m_statusText == text)
        return;
    m_state = state;
    m_statusText = text;
    LOG_MESSAGE(QString("MCP client [%1] state: %2 [%3]")
                    .arg(m_config.name, text, transportToString(m_config.transport)));
    emit stateChanged();
}

} // namespace QodeAssist::Mcp
