// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <functional>
#include <optional>

#include <QString>

#include <LLMQore/AcpPermissionProvider.hpp>

#include "acp/AgentDefinition.hpp"
#include "acp/AgentSpawn.hpp"
#include "completion/CompletionEngine.hpp"
#include "context/IDocumentReader.hpp"
#include "mcp/CompletionMcpServer.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "tools/ProposeCompletionTool.hpp"

namespace QodeAssist {

class AcpCompletionEngine : public CompletionEngine
{
    Q_OBJECT

public:
    using ClientFactory = std::function<
        Acp::AgentProcess(const Acp::AgentDefinition &agent, const QString &cwd, QObject *parent)>;
    using AgentResolver = std::function<std::optional<Acp::AgentDefinition>(const QString &id)>;

    AcpCompletionEngine(
        const Settings::CodeCompletionSettings &completeSettings,
        const Settings::GeneralSettings &generalSettings,
        AgentResolver agentResolver,
        Context::IDocumentReader &documentReader,
        Tools::ProposeCompletionTool *proposeTool,
        QObject *parent = nullptr);

    void setClientFactory(ClientFactory factory);

    void request(quint64 requestId, const CompletionContext &context) override;
    void cancel(quint64 requestId) override;

    bool hasConfiguredAgent() const;

private slots:
    void handleCompletionProposed(
        const QString &filePath, int line, int column, const QString &text);

private:
    struct ActiveRequest
    {
        quint64 requestId = 0;
        QString filePath;
        int line = 0;
        int column = 0;
        QString codeContext;
    };

    void ensureClientAndPrompt();
    void sendPrompt();
    void failActive(const QString &error);
    void releaseClient();
    QString promptText() const;

    const Settings::CodeCompletionSettings &m_completeSettings;
    const Settings::GeneralSettings &m_generalSettings;
    AgentResolver m_agentResolver;
    Context::IDocumentReader &m_documentReader;
    Tools::ProposeCompletionTool *m_proposeTool = nullptr;
    ClientFactory m_clientFactory;
    Mcp::CompletionMcpServer *m_completionServer = nullptr;
    LLMQore::Acp::AcpPermissionProvider *m_permissions = nullptr;

    LLMQore::Acp::AcpClient *m_client = nullptr;
    QString m_boundAgentId;
    QString m_sessionId;
    int m_clientGeneration = 0;
    bool m_sessionReady = false;
    std::optional<ActiveRequest> m_active;
};

} // namespace QodeAssist
