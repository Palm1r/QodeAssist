// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "completion/FimCompletionEngine.hpp"

#include <LLMQore/BaseClient.hpp>
#include <QJsonArray>
#include <QUrl>

#include "completion/CodeHandler.hpp"
#include "context/DocumentContextReader.hpp"
#include "llmcore/ContextData.hpp"
#include "logger/Logger.hpp"
#include "settings/SettingsConstants.hpp"

namespace QodeAssist {

namespace {
constexpr int kSemanticContextTokenBudget = 1000;
}

FimCompletionEngine::FimCompletionEngine(
    const Settings::GeneralSettings &generalSettings,
    const Settings::CodeCompletionSettings &completeSettings,
    Providers::IProviderRegistry &providerRegistry,
    Templates::IPromptProvider *promptProvider,
    Context::IDocumentReader &documentReader,
    Context::ContextManager &contextManager,
    IRequestPerformanceLogger &performanceLogger,
    Context::ICompletionEnricher *enricher,
    QObject *parent)
    : CompletionEngine(parent)
    , m_generalSettings(generalSettings)
    , m_completeSettings(completeSettings)
    , m_providerRegistry(providerRegistry)
    , m_promptProvider(promptProvider)
    , m_documentReader(documentReader)
    , m_contextManager(contextManager)
    , m_performanceLogger(performanceLogger)
    , m_enricher(enricher)
{
}

FimCompletionEngine::~FimCompletionEngine()
{
    const auto requests = m_activeRequests;
    m_activeRequests.clear();
    for (auto it = requests.constBegin(); it != requests.constEnd(); ++it) {
        if (it.value().provider)
            it.value().provider->cancelRequest(it.key());
    }
}

void FimCompletionEngine::request(quint64 requestId, const CompletionContext &context)
{
    auto documentInfo = m_documentReader.readDocument(context.filePath);
    if (!documentInfo.document) {
        const QString error = QString("Document is not available: %1").arg(context.filePath);
        LOG_MESSAGE("Error: " + error);
        emit completionFailed(requestId, error);
        return;
    }

    Context::DocumentContextReader
        reader(documentInfo.document, documentInfo.mimeType, documentInfo.filePath);
    auto updatedContext = reader.prepareContext(context.line, context.column, m_completeSettings);

    bool isPreset1Active = m_contextManager.isSpecifyCompletion(documentInfo);

    const auto providerName = !isPreset1Active ? m_generalSettings.ccProvider()
                                               : m_generalSettings.ccPreset1Provider();
    const auto modelName = !isPreset1Active ? m_generalSettings.ccModel()
                                            : m_generalSettings.ccPreset1Model();
    const auto url = !isPreset1Active ? m_generalSettings.ccUrl()
                                      : m_generalSettings.ccPreset1Url();

    const auto provider = m_providerRegistry.getProviderByName(providerName);

    if (!provider) {
        const QString error = QString("No provider found with name: %1").arg(providerName);
        LOG_MESSAGE(error);
        emit completionFailed(requestId, error);
        return;
    }

    auto templateName = !isPreset1Active ? m_generalSettings.ccTemplate()
                                         : m_generalSettings.ccPreset1Template();

    auto promptTemplate = m_promptProvider->getTemplateByName(templateName);

    if (!promptTemplate) {
        const QString error = QString("No template found with name: %1").arg(templateName);
        LOG_MESSAGE(error);
        emit completionFailed(requestId, error);
        return;
    }

    QJsonObject payload{{"model", modelName}, {"stream", true}};

    const auto stopWords = QJsonArray::fromStringList(promptTemplate->stopWords());
    if (!stopWords.isEmpty())
        payload["stop"] = stopWords;

    QString systemPrompt;
    if (m_completeSettings.useSystemPrompt())
        systemPrompt.append(
            m_completeSettings.useUserMessageTemplateForCC()
                    && promptTemplate->type() == Templates::TemplateType::Chat
                ? m_completeSettings.systemPromptForNonFimModels()
                : m_completeSettings.systemPrompt());

    if (updatedContext.fileContext.has_value())
        systemPrompt.append(updatedContext.fileContext.value());

    if (m_completeSettings.useOpenFilesContext()) {
        if (provider->providerID() == Providers::ProviderID::LlamaCpp) {
            for (const auto openedFilePath : m_contextManager.openedFiles({context.filePath})) {
                if (!updatedContext.filesMetadata) {
                    updatedContext.filesMetadata = QList<LLMCore::FileMetadata>();
                }
                updatedContext.filesMetadata->append({openedFilePath.first, openedFilePath.second});
            }
        } else {
            systemPrompt.append(m_contextManager.openedFilesContext({context.filePath}));
        }
    }

    if (m_enricher && promptTemplate->type() == Templates::TemplateType::Chat
        && m_completeSettings.completionMode.stringValue()
               == QLatin1StringView(Constants::CC_MODE_FIM_WITH_CONTEXT)) {
        const QString enrichment = m_enricher->enrichmentFor(
            documentInfo,
            updatedContext.prefix.value_or(""),
            context.line,
            context.column,
            kSemanticContextTokenBudget);
        if (!enrichment.isEmpty())
            systemPrompt.append(enrichment);
    }

    updatedContext.systemPrompt = systemPrompt;

    if (promptTemplate->type() == Templates::TemplateType::Chat) {
        QString userMessage;
        if (m_completeSettings.useUserMessageTemplateForCC()) {
            userMessage = m_completeSettings.processMessageToFIM(
                updatedContext.prefix.value_or(""), updatedContext.suffix.value_or(""));
        } else {
            userMessage = updatedContext.prefix.value_or("") + updatedContext.suffix.value_or("");
        }

        QVector<LLMCore::Message> messages;
        messages.append({"user", userMessage});
        updatedContext.history = messages;
    }

    provider->prepareRequest(
        payload,
        promptTemplate,
        updatedContext,
        LLMCore::RequestType::CodeCompletion,
        false,
        false);

    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestCompleted,
        this,
        &FimCompletionEngine::handleFullResponse,
        Qt::UniqueConnection);
    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestFinalized,
        this,
        &FimCompletionEngine::handleRequestFinalized,
        Qt::UniqueConnection);
    connect(
        provider->client(),
        &::LLMQore::BaseClient::requestFailed,
        this,
        &FimCompletionEngine::handleRequestFailed,
        Qt::UniqueConnection);

    provider->client()->setTransferTimeout(
        static_cast<int>(m_generalSettings.requestTimeout() * 1000));

    auto llmRequestId
        = provider->sendRequest(QUrl(url), payload, resolveEndpoint(promptTemplate, isPreset1Active));
    m_activeRequests.insert(llmRequestId, {requestId, context.filePath, provider});
    m_performanceLogger.startTimeMeasurement(llmRequestId);
}

void FimCompletionEngine::cancel(quint64 requestId)
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

void FimCompletionEngine::handleFullResponse(
    const ::LLMQore::RequestID &requestId, const QString &fullText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const ActiveRequest active = it.value();
    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);

    emit completionReady(active.requestId, postProcess(fullText, active.filePath));
}

void FimCompletionEngine::handleRequestFinalized(
    const ::LLMQore::RequestID &requestId, const ::LLMQore::CompletionInfo &info)
{
    if (!m_activeRequests.contains(requestId) || !info.usage)
        return;

    const auto &u = *info.usage;
    LOG_MESSAGE(QString("Completion usage [%1]: prompt=%2 completion=%3 cached=%4 reasoning=%5")
                    .arg(requestId)
                    .arg(u.promptTokens)
                    .arg(u.completionTokens)
                    .arg(u.cachedPromptTokens)
                    .arg(u.reasoningTokens));
}

void FimCompletionEngine::handleRequestFailed(
    const ::LLMQore::RequestID &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const ActiveRequest active = it.value();
    m_activeRequests.erase(it);
    m_performanceLogger.endTimeMeasurement(requestId);

    LOG_MESSAGE(QString("Request %1 failed: %2").arg(requestId, error));

    emit completionFailed(active.requestId, error);
}

QString FimCompletionEngine::resolveEndpoint(
    Templates::PromptTemplate *promptTemplate, bool isPreset1Active) const
{
    const QString custom = isPreset1Active ? m_generalSettings.ccPreset1CustomEndpoint()
                                           : m_generalSettings.ccCustomEndpoint();
    return !custom.isEmpty() ? custom : promptTemplate->endpoint();
}

QString FimCompletionEngine::postProcess(const QString &completion, const QString &filePath) const
{
    LOG_MESSAGE(QString("Completions before filter: \n%1").arg(completion));

    const QString outputHandler = m_completeSettings.modelOutputHandler.stringValue();
    QString processedCompletion;

    if (outputHandler == "Raw text") {
        processedCompletion = completion;
    } else if (outputHandler == "Force processing") {
        processedCompletion = CodeHandler::processText(completion, filePath);
    } else {
        processedCompletion = CodeHandler::hasCodeBlocks(completion)
                                  ? CodeHandler::processText(completion, filePath)
                                  : completion;
    }

    if (processedCompletion.endsWith('\n')) {
        QString withoutTrailing = processedCompletion.chopped(1);
        if (!withoutTrailing.contains('\n')) {
            LOG_MESSAGE(QString("Removed trailing newline from single-line completion"));
            processedCompletion = withoutTrailing;
        }
    }

    LOG_MESSAGE(QString("Completion after filter: \n%1").arg(processedCompletion));

    return processedCompletion;
}

} // namespace QodeAssist
