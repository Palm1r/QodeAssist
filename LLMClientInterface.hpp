/* 
 * Copyright (C) 2024 Petr Mironychev
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
#include <llmcore/ContextData.hpp>
#include <llmcore/IPromptProvider.hpp>
#include <llmcore/IProviderRegistry.hpp>
#include <llmcore/RequestHandler.hpp>
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
        LLMCore::IProviderRegistry &providerRegistry,
        LLMCore::IPromptProvider *promptProvider,
        LLMCore::RequestHandlerBase &requestHandler,
        Context::IDocumentReader &documentReader,
        IRequestPerformanceLogger &performanceLogger);

    Utils::FilePath serverDeviceTemplate() const override;

    void sendCompletionToClient(
        const QString &completion, const QJsonObject &request, bool isComplete);

    void handleCompletion(const QJsonObject &request);

    // exposed for tests
    void sendData(const QByteArray &data) override;

protected:
    void startImpl() override;
    void parseCurrentMessage() override;

private:
    void handleInitialize(const QJsonObject &request);
    void handleShutdown(const QJsonObject &request);
    void handleTextDocumentDidOpen(const QJsonObject &request);
    void handleInitialized(const QJsonObject &request);
    void handleExit(const QJsonObject &request);
    void handleCancelRequest(const QJsonObject &request);

    LLMCore::ContextData prepareContext(
        const QJsonObject &request, const Context::DocumentInfo &documentInfo);

    const Settings::CodeCompletionSettings &m_completeSettings;
    const Settings::GeneralSettings &m_generalSettings;
    LLMCore::IPromptProvider *m_promptProvider = nullptr;
    LLMCore::IProviderRegistry &m_providerRegistry;
    LLMCore::RequestHandlerBase &m_requestHandler;
    Context::IDocumentReader &m_documentReader;
    IRequestPerformanceLogger &m_performanceLogger;
    QElapsedTimer m_completionTimer;
    Context::ContextManager *m_contextManager;
};

} // namespace QodeAssist
