/* 
 * Copyright (C) 2024-2025 Petr Mironychev
 *
 * This file is part of QodeAssist.
 *
 * QodeAssist is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QodeAssist is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QodeAssist. If not, see <https://www.gnu.org/licenses/>.
 */

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
