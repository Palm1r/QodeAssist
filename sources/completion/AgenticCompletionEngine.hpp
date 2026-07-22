// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QHash>

#include "completion/CompletionEngine.hpp"
#include "context/IDocumentReader.hpp"
#include "logger/IRequestPerformanceLogger.hpp"
#include "providers/IProviderRegistry.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "templates/IPromptProvider.hpp"
#include "tools/ProposeCompletionTool.hpp"

namespace QodeAssist {

class AgenticCompletionEngine : public CompletionEngine
{
    Q_OBJECT

public:
    AgenticCompletionEngine(
        const Settings::GeneralSettings &generalSettings,
        const Settings::CodeCompletionSettings &completeSettings,
        Providers::IProviderRegistry &providerRegistry,
        Templates::IPromptProvider *promptProvider,
        Context::IDocumentReader &documentReader,
        IRequestPerformanceLogger &performanceLogger,
        QObject *parent = nullptr);
    ~AgenticCompletionEngine() override;

    void request(quint64 requestId, const CompletionContext &context) override;
    void cancel(quint64 requestId) override;

private slots:
    void handleCompletionProposed(
        const QString &filePath, int line, int column, const QString &text);
    void handleFullResponse(const ::LLMQore::RequestID &requestId, const QString &fullText);
    void handleRequestFailed(const ::LLMQore::RequestID &requestId, const QString &error);

private:
    struct ActiveRequest
    {
        quint64 requestId = 0;
        QString filePath;
        Providers::Provider *provider = nullptr;
    };

    const Settings::GeneralSettings &m_generalSettings;
    const Settings::CodeCompletionSettings &m_completeSettings;
    Providers::IProviderRegistry &m_providerRegistry;
    Templates::IPromptProvider *m_promptProvider = nullptr;
    Context::IDocumentReader &m_documentReader;
    IRequestPerformanceLogger &m_performanceLogger;
    QHash<::LLMQore::RequestID, ActiveRequest> m_activeRequests;
};

} // namespace QodeAssist
