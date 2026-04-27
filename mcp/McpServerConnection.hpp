// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace LLMQore::Mcp {
class McpClient;
class McpTransport;
} // namespace LLMQore::Mcp

namespace QodeAssist::PluginLLMCore {
class Provider;
}

namespace QodeAssist::Mcp {

enum class McpTransportKind { Http, Stdio };

struct McpServerConfig
{
    QString name;
    bool enabled = true;

    McpTransportKind transport = McpTransportKind::Http;

    // HTTP/SSE transport
    QUrl url;
    QHash<QString, QString> headers;
    QString spec; // MCP protocol spec, e.g. "2024-11-05"; empty = Latest.

    // Stdio transport
    QString command;
    QStringList args;
    QHash<QString, QString> env;
    QString workingDirectory;

    static McpServerConfig fromJson(const QString &name, const QJsonObject &obj);
    QJsonObject toJson() const;
};

enum class McpConnectionState { Disabled, Connecting, Connected, Failed };

class McpServerConnection : public QObject
{
    Q_OBJECT
public:
    explicit McpServerConnection(McpServerConfig config, QObject *parent = nullptr);
    ~McpServerConnection() override;

    const McpServerConfig &config() const { return m_config; }
    McpConnectionState state() const { return m_state; }
    QString statusText() const { return m_statusText; }
    int toolCount() const { return m_toolIds.size(); }
    QStringList toolNames() const { return m_toolIds; }

    void setProviders(const QList<PluginLLMCore::Provider *> &providers);

    void connectToServer();
    void disconnectFromServer();

signals:
    void stateChanged();

private:
    void setState(McpConnectionState state, const QString &text = {});
    void fetchAndRegisterTools();
    void registerTools(const QList<::LLMQore::Mcp::McpClient *> & /*unused*/);
    void unregisterTools();
    ::LLMQore::Mcp::McpTransport *createTransport();

    McpServerConfig m_config;
    McpConnectionState m_state = McpConnectionState::Disabled;
    QString m_statusText;

    QPointer<::LLMQore::Mcp::McpClient> m_client;
    QPointer<::LLMQore::Mcp::McpTransport> m_transport;
    QPointer<QTimer> m_listToolsWatchdog;

    QList<QPointer<PluginLLMCore::Provider>> m_providers;
    QStringList m_toolIds;
};

} // namespace QodeAssist::Mcp
