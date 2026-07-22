// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "completion/AgenticCompletionEngine.hpp"

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ToolsManager.hpp>
#include <QUrl>

#include "context/DocumentContextReader.hpp"
#include "llmcore/ContextData.hpp"
#include "logger/Logger.hpp"
#include "settings/ToolsSettings.hpp"

namespace QodeAssist {

AgenticCompletionEngine::AgenticCompletionEngine(
    const Settings::GeneralSettings &generalSettings,
    const Settings::CodeCompletionSettings &completeSettings,
    Providers::IProviderRegistry &providerRegistry,
    Templates::IPromptProvider *promptProvider,
    Context::IDocumentReader &documentReader,
    IRequestPerformanceLogger &performanceLogger,
    QObject *parent)
    : CompletionEngine(parent)
    , m_generalSettings(generalSettings)
    , m_completeSettings(completeSettings)
    , m_providerRegistry(providerRegistry)
    , m_promptProvider(promptProvider)
    , m_documentReader(documentReader)
    , m_performanceLogger(performanceLogger)
{
}

AgenticCompletionEngine::~AgenticCompletionEngine()
{
    const auto requests = m_activeRequests;
    m_activeRequests.clear();
    for (auto it = requests.constBegin(); it != requests.constEnd(); ++it) {
        if (it.value().provider)
            it.value().provider->cancelRequest(it.key());
    }
}

void AgenticCompletionEngine::request(quint64 requestId, const CompletionContext &context)
{
    auto documentInfo = m_documentReader.readDocument(context.filePath);
    if (!documentInfo.document) {
        const QString error = QString("Document is not available: %1").arg(context.filePath);
        LOG_MESSAGE("Error: " + error);
        emit completionFailed(requestId, error);
        return;
    }

    const auto providerName = m_generalSettings.ccProvider();
    const auto provider = m_providerRegistry.getProviderByName(providerName);

    if (!provider) {
        const QString error = QString("No provider found with name: %1").arg(providerName);
        LOG_MESSAGE(error);
        emit completionFailed(requestId, error);
        return;
    }

    if (!provider->capabilities().testFlag(Providers::ProviderCapability::Tools)) {
        const QString error
            = QString("Agentic completion needs tool calling, which provider %1 does not support")
                  .arg(providerName);
        LOG_MESSAGE(error);
        emit completionFailed(requestId, error);
        return;
    }

    auto promptTemplate = m_promptProvider->getTemplateByName(m_generalSettings.ccTemplate());

    if (!promptTemplate) {
        const QString error
            = QString("No template found with name: %1").arg(m_generalSettings.ccTemplate());
        LOG_MESSAGE(error);
        emit completionFailed(requestId, error);
        return;
    }

    if (promptTemplate->type() != Templates::TemplateType::Chat) {
        const QString error
            = QString("Agentic completion needs a chat-type template; %1 is FIM-only")
                  .arg(promptTemplate->name());
        LOG_MESSAGE(error);
        emit completionFailed(requestId, error);
        return;
    }

    if (auto *toolsManager = provider->toolsManager()) {
        auto *proposeTool = qobject_cast<Tools::ProposeCompletionTool *>(
            toolsManager->tool(Tools::ProposeCompletionTool::toolId()));
        if (!proposeTool) {
            proposeTool = new Tools::ProposeCompletionTool();
            toolsManager->addTool(proposeTool);
        }
        connect(
            proposeTool,
            &Tools::ProposeCompletionTool::completionProposed,
            this,
            &AgenticCompletionEngine::handleCompletionProposed,
            Qt::UniqueConnection);
    }

    Context::DocumentContextReader
        reader(documentInfo.document, documentInfo.mimeType, documentInfo.filePath);
    auto updatedContext = reader.prepareContext(context.line, context.column, m_completeSettings);

    QString systemPrompt = m_completeSettings.systemPromptForNonFimModels();
    systemPrompt.append(
        QString("\n\nYou are completing code at %1, zero-based line %2, column %3. "
                "You may call the read-only project tools to gather context. When you know the "
                "completion, call the propose_completion tool exactly once with the completion "
                "text for that position; the text is shown to the user as an inline suggestion. "
                "Do not answer in prose and do not edit any files.")
            .arg(documentInfo.filePath)
            .arg(context.line)
            .arg(context.column));

    if (updatedContext.fileContext.has_value())
        systemPrompt.append(updatedContext.fileContext.value());

    updatedContext.systemPrompt = systemPrompt;

    QString userMessage = m_completeSettings.processMessageToFIM(
        updatedContext.prefix.value_or(""), updatedContext.suffix.value_or(""));
    QVector<LLMCore::Message> messages;
    messages.append({"user", userMessage});
    updatedContext.history = messages;

    QJsonObject payload{{"model", m_generalSettings.ccModel()}, {"stream", true}};

    provider->prepareRequest(
        payload,
        promptTemplate,
        updatedContext,
        LLMCore::RequestType::CodeCompletion,
        true,
        false);

    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestCompleted,
        this,
        &AgenticCompletionEngine::handleFullResponse,
        Qt::UniqueConnection);
    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestFailed,
        this,
        &AgenticCompletionEngine::handleRequestFailed,
        Qt::UniqueConnection);

    provider->client()->setMaxToolContinuations(Settings::toolsSettings().maxToolContinuations());
    provider->client()->setTransferTimeout(
        static_cast<int>(m_generalSettings.requestTimeout() * 1000));

    auto llmRequestId = provider->sendRequest(
        QUrl(m_generalSettings.ccUrl()), payload, promptTemplate->endpoint());
    m_activeRequests.insert(llmRequestId, {requestId, documentInfo.filePath, provider});
    m_performanceLogger.startTimeMeasurement(llmRequestId);
}

void AgenticCompletionEngine::cancel(quint64 requestId)
{
    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        if (it.value().requestId != requestId)
            continue;
        const auto llmRequestId = it.key();
        auto *provider = it.value().provider;
        m_activeRequests.erase(it);
        m_performanceLogger.endTimeMeasurement(llmRequestId);
        if (provider)
            provider->cancelRequest(llmRequestId);
        return;
    }
}

void AgenticCompletionEngine::handleCompletionProposed(
    const QString &filePath, int line, int column, const QString &text)
{
    Q_UNUSED(line)
    Q_UNUSED(column)

    auto match = m_activeRequests.end();
    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        if (it.value().filePath == filePath) {
            match = it;
            break;
        }
    }

    if (match == m_activeRequests.end() && m_activeRequests.size() == 1)
        match = m_activeRequests.begin();

    if (match == m_activeRequests.end()) {
        LOG_MESSAGE(
            QString("Ignoring completion proposal for %1: no completion request in flight")
                .arg(filePath));
        return;
    }

    const quint64 externalId = match.value().requestId;
    const auto llmRequestId = match.key();
    m_activeRequests.erase(match);
    m_performanceLogger.endTimeMeasurement(llmRequestId);
    emit completionReady(externalId, text);
}

void AgenticCompletionEngine::handleFullResponse(
    const ::LLMQore::RequestID &requestId, const QString &fullText)
{
    Q_UNUSED(fullText)

    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const ActiveRequest active = it.value();
    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);

    const QString error
        = QStringLiteral("The model finished without proposing a completion");
    LOG_MESSAGE(QString("Agentic completion request %1: %2").arg(requestId, error));
    emit completionFailed(active.requestId, error);
}

void AgenticCompletionEngine::handleRequestFailed(
    const ::LLMQore::RequestID &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const ActiveRequest active = it.value();
    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);

    LOG_MESSAGE(QString("Agentic completion request %1 failed: %2").arg(requestId, error));

    emit completionFailed(active.requestId, error);
}

} // namespace QodeAssist
