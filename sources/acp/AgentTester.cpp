// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "AgentTester.hpp"

#include <chrono>

#include <LLMQore/AcpClient.hpp>
#include <LLMQore/RpcStdioTransport.hpp>

#include "acp/AgentSpawn.hpp"

namespace QodeAssist::Acp {

namespace {

constexpr int maxStderrLines = 20;

QString capabilityList(const LLMQore::Acp::AgentCapabilities &capabilities)
{
    QStringList prompt;
    if (capabilities.promptCapabilities.image)
        prompt.append(AgentTester::tr("image"));
    if (capabilities.promptCapabilities.audio)
        prompt.append(AgentTester::tr("audio"));
    if (capabilities.promptCapabilities.embeddedContext)
        prompt.append(AgentTester::tr("embedded context"));
    return prompt.join(QStringLiteral(", "));
}

QString mcpList(const LLMQore::Acp::AgentCapabilities &capabilities)
{
    QStringList transports;
    if (capabilities.mcpCapabilities.http)
        transports.append(QStringLiteral("http"));
    if (capabilities.mcpCapabilities.sse)
        transports.append(QStringLiteral("sse"));
    return transports.join(QStringLiteral(", "));
}

QString describe(const AgentDefinition &agent, const LLMQore::Acp::InitializeResult &result)
{
    const QString name = result.agentInfo && !result.agentInfo->name.isEmpty()
                             ? result.agentInfo->name
                             : agent.name;
    const QString version = result.agentInfo && !result.agentInfo->version.isEmpty()
                                ? result.agentInfo->version
                                : agent.version;

    QStringList lines;
    lines.append(version.isEmpty() ? name : QStringLiteral("%1 %2").arg(name, version));
    lines.append(AgentTester::tr("Protocol version: %1").arg(result.protocolVersion));
    lines.append(
        AgentTester::tr("Session persistence (loadSession): %1")
            .arg(
                result.agentCapabilities.loadSession ? AgentTester::tr("supported")
                                                     : AgentTester::tr("not supported")));

    const QString prompt = capabilityList(result.agentCapabilities);
    lines.append(
        AgentTester::tr("Prompt content: %1")
            .arg(prompt.isEmpty() ? AgentTester::tr("text only") : prompt));

    const QString mcp = mcpList(result.agentCapabilities);
    if (!mcp.isEmpty())
        lines.append(AgentTester::tr("MCP transports: %1").arg(mcp));

    if (result.authMethods.isEmpty()) {
        lines.append(AgentTester::tr("Authentication: not required"));
    } else {
        QStringList methods;
        for (const LLMQore::Acp::AuthMethod &method : result.authMethods)
            methods.append(method.name.isEmpty() ? method.id : method.name);
        lines.append(AgentTester::tr("Authentication: %1").arg(methods.join(QStringLiteral(", "))));
    }

    return lines.join(QLatin1Char('\n'));
}

} // namespace

AgentTester::AgentTester(QObject *parent)
    : QObject(parent)
{}

void AgentTester::start(const AgentDefinition &agent, const QString &workingDirectory)
{
    if (m_running)
        return;

    const AgentProcess process = spawnAgent(agent, workingDirectory, this);
    if (!process.client) {
        emit finished(false, tr("This agent has no launchable distribution."));
        return;
    }

    m_stderr.clear();
    m_runner = process.command;
    m_running = true;

    m_client = process.client;
    m_transport = qobject_cast<LLMQore::Rpc::StdioClientTransport *>(m_client->transport());

    connect(m_client, &LLMQore::Acp::AcpClient::agentStderr, this, [this](const QString &line) {
        if (m_stderr.size() < maxStderrLines)
            m_stderr.append(line);
    });
    connect(m_client, &LLMQore::Acp::AcpClient::errorOccurred, this, [this](const QString &error) {
        report(false, error);
    });

    m_client->connectAndInitialize(std::chrono::seconds(60))
        .then(
            this,
            [this, agent](const LLMQore::Acp::InitializeResult &result) {
                report(true, describe(agent, result));
            })
        .onFailed(this, [this](const std::exception &e) {
            report(false, QString::fromUtf8(e.what()));
        });
}

void AgentTester::cancel()
{
    if (m_running)
        report(false, tr("Test cancelled."));
}

void AgentTester::report(bool ok, const QString &text)
{
    if (!m_running)
        return;

    m_running = false;
    const QString details = ok ? text : text + diagnostics();
    releaseClient();
    emit finished(ok, details);
}

QString AgentTester::diagnostics() const
{
    QString result;

    const QString hint = runnerHint(m_runner);
    if (!hint.isEmpty())
        result += QLatin1Char('\n') + hint;

    if (!m_stderr.isEmpty()) {
        result += QLatin1Char('\n') + tr("Agent output:") + QLatin1Char('\n')
                  + m_stderr.join(QLatin1Char('\n'));
    }

    return result;
}

void AgentTester::releaseClient()
{
    LLMQore::Acp::AcpClient *client = m_client;
    LLMQore::Rpc::StdioClientTransport *transport = m_transport;
    m_client = nullptr;
    m_transport = nullptr;

    if (client)
        client->disconnect(this);

    QMetaObject::invokeMethod(
        this,
        [client, transport]() {
            if (client) {
                client->shutdown();
                client->deleteLater();
            }
            if (transport)
                transport->deleteLater();
        },
        Qt::QueuedConnection);
}

} // namespace QodeAssist::Acp
