// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QHash>

#include "completion/CompletionEngine.hpp"
#include "context/CompletionContextEnricher.hpp"
#include "context/ContextManager.hpp"
#include "context/IDocumentReader.hpp"
#include "logger/IRequestPerformanceLogger.hpp"
#include "providers/IProviderRegistry.hpp"
#include "settings/CodeCompletionSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "templates/IPromptProvider.hpp"

namespace QodeAssist {

class FimCompletionEngine : public CompletionEngine
{
    Q_OBJECT

public:
    FimCompletionEngine(
        const Settings::GeneralSettings &generalSettings,
        const Settings::CodeCompletionSettings &completeSettings,
        Providers::IProviderRegistry &providerRegistry,
        Templates::IPromptProvider *promptProvider,
        Context::IDocumentReader &documentReader,
        Context::ContextManager &contextManager,
        IRequestPerformanceLogger &performanceLogger,
        Context::ICompletionEnricher *enricher = nullptr,
        QObject *parent = nullptr);
    ~FimCompletionEngine() override;

    void request(quint64 requestId, const CompletionContext &context) override;
    void cancel(quint64 requestId) override;

private slots:
    void handleFullResponse(const ::LLMQore::RequestID &requestId, const QString &fullText);
    void handleRequestFinalized(
        const ::LLMQore::RequestID &requestId, const ::LLMQore::CompletionInfo &info);
    void handleRequestFailed(const ::LLMQore::RequestID &requestId, const QString &error);

private:
    struct ActiveRequest
    {
        quint64 requestId = 0;
        QString filePath;
        Providers::Provider *provider = nullptr;
    };

    QString resolveEndpoint(Templates::PromptTemplate *promptTemplate, bool isPreset1Active) const;
    QString postProcess(const QString &completion, const QString &filePath) const;

    const Settings::GeneralSettings &m_generalSettings;
    const Settings::CodeCompletionSettings &m_completeSettings;
    Providers::IProviderRegistry &m_providerRegistry;
    Templates::IPromptProvider *m_promptProvider = nullptr;
    Context::IDocumentReader &m_documentReader;
    Context::ContextManager &m_contextManager;
    IRequestPerformanceLogger &m_performanceLogger;
    Context::ICompletionEnricher *m_enricher = nullptr;
    QHash<::LLMQore::RequestID, ActiveRequest> m_activeRequests;
};

} // namespace QodeAssist
