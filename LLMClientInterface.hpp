// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <languageclient/languageclientinterface.h>
#include <texteditor/texteditor.h>

#include <context/ContextManager.hpp>
#include <context/IDocumentReader.hpp>
#include <context/ProgrammingLanguage.hpp>
#include <pluginllmcore/ContextData.hpp>
#include <pluginllmcore/IPromptProvider.hpp>
#include <pluginllmcore/IProviderRegistry.hpp>
#include <logger/IRequestPerformanceLogger.hpp>
#include <settings/CodeCompletionSettings.hpp>
#include <settings/GeneralSettings.hpp>

class QNetworkReply;
class QNetworkAccessManager;

namespace QodeAssist {

class LLMClientInterface : public LanguageClient::BaseClientInterface
{
    Q_OBJECT

public:
    LLMClientInterface(
        const Settings::GeneralSettings &generalSettings,
        const Settings::CodeCompletionSettings &completeSettings,
        PluginLLMCore::IProviderRegistry &providerRegistry,
        PluginLLMCore::IPromptProvider *promptProvider,
        Context::IDocumentReader &documentReader,
        IRequestPerformanceLogger &performanceLogger);
    ~LLMClientInterface() override;

    Utils::FilePath serverDeviceTemplate() const override;

    void sendCompletionToClient(
        const QString &completion, const QJsonObject &request, bool isComplete);

    void handleCompletion(const QJsonObject &request);

    // exposed for tests
    void sendData(const QByteArray &data) override;

    Context::ContextManager *contextManager() const;

protected:
    void startImpl() override;

private slots:
    void handleFullResponse(const QString &requestId, const QString &fullText);
    void handleRequestFailed(const QString &requestId, const QString &error);

private:
    void handleInitialize(const QJsonObject &request);
    void handleShutdown(const QJsonObject &request);
    void handleTextDocumentDidOpen(const QJsonObject &request);
    void handleInitialized(const QJsonObject &request);
    void handleExit(const QJsonObject &request);
    void handleCancelRequest();
    void sendErrorResponse(const QJsonObject &request, const QString &errorMessage);

    struct RequestContext
    {
        QJsonObject originalRequest;
        PluginLLMCore::Provider *provider;
    };

    PluginLLMCore::ContextData prepareContext(
        const QJsonObject &request, const Context::DocumentInfo &documentInfo);

    QString resolveEndpoint(
        PluginLLMCore::PromptTemplate *promptTemplate, bool isLanguageSpecify) const;

    const Settings::CodeCompletionSettings &m_completeSettings;
    const Settings::GeneralSettings &m_generalSettings;
    PluginLLMCore::IPromptProvider *m_promptProvider = nullptr;
    PluginLLMCore::IProviderRegistry &m_providerRegistry;
    Context::IDocumentReader &m_documentReader;
    IRequestPerformanceLogger &m_performanceLogger;
    QElapsedTimer m_completionTimer;
    Context::ContextManager *m_contextManager;
    QHash<QString, RequestContext> m_activeRequests;
};

} // namespace QodeAssist
