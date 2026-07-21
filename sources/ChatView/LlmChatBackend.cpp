// Copyright (C) 2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "LlmChatBackend.hpp"

#include <LLMQore/ToolsManager.hpp>

#include <QPromise>

#include <memory>

#include "ChatFileStore.hpp"
#include "llmcore/ContextData.hpp"
#include "logger/Logger.hpp"
#include "providers/ProvidersManager.hpp"
#include "session/FencedText.hpp"
#include "session/FileEditPayload.hpp"
#include "session/HistoryProjection.hpp"
#include "settings/ChatAssistantSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/ToolsSettings.hpp"
#include "tools/ReadOriginalHistoryTool.hpp"
#include "tools/TodoTool.hpp"

namespace QodeAssist::Chat {

LlmChatBackend::LlmChatBackend(Templates::IPromptProvider *promptProvider, QObject *parent)
    : Session::ChatBackend(parent)
    , m_promptProvider(promptProvider)
    , m_providerResolver([](const QString &name) {
        return Providers::ProvidersManager::instance().getProviderByName(name);
    })
{}

void LlmChatBackend::setProviderResolver(ProviderResolver resolver)
{
    m_providerResolver = std::move(resolver);
}

LlmChatBackend::~LlmChatBackend()
{
    cancel();
}

void LlmChatBackend::sendTurn(const Session::TurnRequest &request)
{
    cancel();

    if (!request.history) {
        emit sessionEvent(
            Session::TurnFailed{.turnId = {}, .error = tr("Chat turn sent without a conversation")});
        return;
    }

    const auto providerName = Settings::generalSettings().caProvider();
    auto *provider = m_providerResolver(providerName);

    if (!provider) {
        const QString error = tr("No provider found with name: %1").arg(providerName);
        LOG_MESSAGE(error);
        emit sessionEvent(Session::TurnFailed{.turnId = {}, .error = error});
        return;
    }

    const auto templateName = Settings::generalSettings().caTemplate();
    auto *promptTemplate = m_promptProvider->getTemplateByName(templateName);

    if (!promptTemplate) {
        const QString error = tr("No template found with name: %1").arg(templateName);
        LOG_MESSAGE(error);
        emit sessionEvent(Session::TurnFailed{.turnId = {}, .error = error});
        return;
    }

    LLMCore::ContextData context;
    if (request.context && Settings::chatAssistantSettings().useSystemPrompt())
        context.systemPrompt = Session::renderSystemPrompt(*request.context);
    context.history = renderHistory(*request.history, provider, promptTemplate);

    QJsonObject payload{{"model", Settings::generalSettings().caModel()}, {"stream", true}};

    provider->prepareRequest(
        payload,
        promptTemplate,
        context,
        LLMCore::RequestType::Chat,
        Settings::chatAssistantSettings().enableChatTools(),
        Settings::chatAssistantSettings().enableThinkingMode());

    provider->client()->setMaxToolContinuations(Settings::toolsSettings().maxToolContinuations());
    provider->client()->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    connectClient(provider);

    const QString customEndpoint = Settings::generalSettings().caCustomEndpoint();
    const QString endpoint = !customEndpoint.isEmpty() ? customEndpoint
                                                       : promptTemplate->endpoint();

    m_provider = provider;
    m_dropPreToolText = !promptTemplate->supportsToolHistory();
    const QString requestId = m_ledger.beginTurn(
        provider->sendRequest(QUrl(Settings::generalSettings().caUrl()), payload, endpoint));

    emit sessionEvent(Session::TurnStarted{.turnId = requestId});

    bindToolSessions(provider);
}

void LlmChatBackend::cancel()
{
    if (!m_provider)
        return;

    auto *provider = m_provider;
    const QString requestId = m_ledger.activeTurnId();

    releaseRequest();

    if (!requestId.isEmpty())
        provider->cancelRequest(requestId);

    LOG_MESSAGE("Chat request cancelled and state cleared");
}

void LlmChatBackend::releaseRequest()
{
    cancelPendingPermissions();

    if (m_provider) {
        disconnect(m_provider->client(), nullptr, this, nullptr);
        if (m_provider->toolsManager())
            m_provider->toolsManager()->setExecutionGate({});
    }

    m_provider = nullptr;
    m_dropPreToolText = false;
}

Session::TurnContextNeeds LlmChatBackend::contextNeeds() const
{
    return {Settings::chatAssistantSettings().useSystemPrompt()};
}

void LlmChatBackend::setChatFilePath(const QString &filePath)
{
    m_chatFilePath = filePath;
}

void LlmChatBackend::clearToolSession(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    const auto providerName = Settings::generalSettings().caProvider();
    auto *provider = m_providerResolver(providerName);

    if (!provider || !provider->capabilities().testFlag(Providers::ProviderCapability::Tools)
        || !provider->toolsManager()) {
        return;
    }

    if (auto *todoTool = qobject_cast<Tools::TodoTool *>(
            provider->toolsManager()->tool("todo_tool"))) {
        todoTool->clearSession(filePath);
    }
}

void LlmChatBackend::connectClient(Providers::Provider *provider)
{
    auto *client = provider->client();

    connect(
        client,
        &::LLMQore::BaseClient::chunkReceived,
        this,
        &LlmChatBackend::handleChunk,
        Qt::UniqueConnection);
    connect(
        client,
        &::LLMQore::BaseClient::requestCompleted,
        this,
        &LlmChatBackend::handleCompleted,
        Qt::UniqueConnection);
    connect(
        client,
        &::LLMQore::BaseClient::requestFinalized,
        this,
        &LlmChatBackend::handleFinalized,
        Qt::UniqueConnection);
    connect(
        client,
        &::LLMQore::BaseClient::requestFailed,
        this,
        &LlmChatBackend::handleFailed,
        Qt::UniqueConnection);
    connect(
        client,
        &::LLMQore::BaseClient::toolStarted,
        this,
        &LlmChatBackend::handleToolStarted,
        Qt::UniqueConnection);
    connect(
        client,
        &::LLMQore::BaseClient::toolResultReady,
        this,
        &LlmChatBackend::handleToolResult,
        Qt::UniqueConnection);
    connect(
        client,
        &::LLMQore::BaseClient::thinkingBlockReceived,
        this,
        &LlmChatBackend::handleThinkingBlock,
        Qt::UniqueConnection);
}

void LlmChatBackend::bindToolSessions(Providers::Provider *provider)
{
    if (!provider->capabilities().testFlag(Providers::ProviderCapability::Tools)
        || !provider->toolsManager()) {
        return;
    }

    if (auto *todoTool = qobject_cast<Tools::TodoTool *>(
            provider->toolsManager()->tool("todo_tool"))) {
        todoTool->setCurrentSessionId(m_chatFilePath);
    }
    if (auto *historyTool = qobject_cast<Tools::ReadOriginalHistoryTool *>(
            provider->toolsManager()->tool("read_original_history"))) {
        historyTool->setCurrentSessionId(m_chatFilePath);
    }

    installExecutionGate(provider);
}

void LlmChatBackend::installExecutionGate(Providers::Provider *provider)
{
    provider->toolsManager()->setExecutionGate(
        [this](
            const QString &requestId,
            const QString &toolId,
            const QString &toolName,
            const QJsonObject &input) {
            return gateToolExecution(requestId, toolId, toolName, input);
        });
}

QFuture<bool> LlmChatBackend::gateToolExecution(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &input)
{
    const auto allow = [] {
        QPromise<bool> promise;
        promise.start();
        promise.addResult(true);
        promise.finish();
        return promise.future();
    };

    if (!m_ledger.isActiveTurn(requestId))
        return allow();

    if (!m_provider || !m_provider->toolsManager())
        return allow();

    auto *tool = m_provider->toolsManager()->tool(toolName);
    if (tool && tool->safety() == ::LLMQore::ToolSafety::ReadOnly)
        return allow();

    auto promise = std::make_shared<QPromise<bool>>();
    promise->start();

    const QString permissionId = m_ledger.registerPermission(
        [promise](const QString &optionId) {
            const bool allowed = optionId == Session::PermissionOptionKind::AllowOnce
                                 || optionId == Session::PermissionOptionKind::AllowAlways;
            promise->addResult(allowed);
            promise->finish();
        },
        [promise] {
            promise->addResult(false);
            promise->finish();
        });

    QFuture<bool> decision = promise->future();

    const QString title = tool && !tool->displayName().isEmpty()
                              ? tr("Run %1").arg(tool->displayName())
                              : tr("Run %1").arg(toolName);

    emit sessionEvent(
        Session::PermissionRequested{
            .turnId = requestId,
            .requestId = permissionId,
            .toolCallId = toolId,
            .title = title,
            .toolKind = toolName,
            .options
            = {Session::PermissionOption{"allow_once", tr("Allow"), "allow_once"},
               Session::PermissionOption{
                   "allow_always", tr("Allow for this conversation"), "allow_always"},
               Session::PermissionOption{"reject_once", tr("Don't run it"), "reject_once"}}});

    Q_UNUSED(input)
    return decision;
}

bool LlmChatBackend::respondPermission(const QString &requestId, const QString &optionId)
{
    if (!m_ledger.resolvePermission(requestId, optionId))
        return false;

    emit sessionEvent(
        Session::PermissionResolved{
            .turnId = m_ledger.activeTurnId(), .requestId = requestId, .optionId = optionId});

    return true;
}

void LlmChatBackend::cancelPendingPermissions()
{
    const QString turnId = m_ledger.activeTurnId();
    const QStringList cancelled = m_ledger.endTurn();

    for (const QString &requestId : cancelled) {
        emit sessionEvent(
            Session::PermissionResolved{
                .turnId = turnId, .requestId = requestId, .cancelled = true});
    }
}

QVector<LLMCore::Message> LlmChatBackend::renderHistory(
    const Session::ConversationHistory &history,
    Providers::Provider *provider,
    Templates::PromptTemplate *promptTemplate) const
{
    const bool toolHistory = promptTemplate->supportsToolHistory();

    QVector<LLMCore::Message> messages;
    int toolCallMsgIdx = -1;

    for (const Session::MessageRow &row : Session::projectToRows(history)) {
        const Session::RowTreatment treatment
            = Session::rowTreatmentFor(Session::RowAudience::Prompt, row.kind);

        if (treatment == Session::RowTreatment::ToolExchange) {
            if (!toolHistory || row.toolName.isEmpty())
                continue;

            if (toolCallMsgIdx < 0) {
                LLMCore::Message assistantCall;
                assistantCall.role = "assistant";
                messages.append(assistantCall);
                toolCallMsgIdx = messages.size() - 1;
            }

            LLMCore::ToolCall call;
            call.id = row.id;
            call.name = row.toolName;
            call.arguments = row.toolArguments;
            messages[toolCallMsgIdx].toolCalls.append(call);

            LLMCore::Message toolResult;
            toolResult.role = "tool";
            toolResult.toolCallId = row.id;
            toolResult.toolName = row.toolName;
            toolResult.content = row.toolResult;
            messages.append(toolResult);
            continue;
        }

        toolCallMsgIdx = -1;

        if (treatment == Session::RowTreatment::Omit)
            continue;

        LLMCore::Message apiMessage;
        apiMessage.role = treatment == Session::RowTreatment::UserText ? "user" : "assistant";
        apiMessage.content = row.content;

        if (!row.attachments.isEmpty() && !m_chatFilePath.isEmpty()) {
            apiMessage.content += "\n\nAttached files:";
            for (const Session::AttachmentBlock &attachment : row.attachments) {
                const QByteArray fileContent = ChatFileStore::loadRawContentFromStorage(
                    m_chatFilePath, attachment.storedPath);
                if (fileContent.isEmpty())
                    continue;

                apiMessage.content
                    += "\n\n"
                       + Session::fencedFileBlock(
                           attachment.fileName, QString::fromUtf8(fileContent));
            }
        }

        apiMessage.isThinking = treatment == Session::RowTreatment::AssistantThinking;
        apiMessage.isRedacted = row.redacted;
        apiMessage.signature = row.signature;

        if (provider->capabilities().testFlag(Providers::ProviderCapability::Image)
            && !m_chatFilePath.isEmpty() && !row.images.isEmpty()) {
            const auto apiImages = loadImagesFromStorage(row.images);
            if (!apiImages.isEmpty())
                apiMessage.images = apiImages;
        }

        messages.append(apiMessage);
    }

    return messages;
}

QVector<LLMCore::ImageAttachment> LlmChatBackend::loadImagesFromStorage(
    const QList<Session::ImageBlock> &storedImages) const
{
    QVector<LLMCore::ImageAttachment> apiImages;

    for (const Session::ImageBlock &storedImage : storedImages) {
        const QString base64Data
            = ChatFileStore::loadContentFromStorage(m_chatFilePath, storedImage.storedPath);
        if (base64Data.isEmpty()) {
            LOG_MESSAGE(QString("Warning: Failed to load image: %1").arg(storedImage.storedPath));
            continue;
        }

        LLMCore::ImageAttachment apiImage;
        apiImage.data = base64Data;
        apiImage.mediaType = storedImage.mediaType;
        apiImage.isUrl = false;

        apiImages.append(apiImage);
    }

    return apiImages;
}

void LlmChatBackend::handleChunk(const QString &requestId, const QString &chunk)
{
    if (!m_ledger.isActiveTurn(requestId))
        return;

    emit sessionEvent(Session::TextDelta{.turnId = requestId, .text = chunk});
}

void LlmChatBackend::handleCompleted(const QString &requestId, const QString &fullText)
{
    if (!m_ledger.isActiveTurn(requestId))
        return;

    LOG_MESSAGE(
        QString("Chat request %1 completed, %2 characters").arg(requestId).arg(fullText.length()));

    releaseRequest();

    emit sessionEvent(Session::TurnCompleted{.turnId = requestId});
}

void LlmChatBackend::handleFinalized(
    const ::LLMQore::RequestID &requestId, const ::LLMQore::CompletionInfo &info)
{
    if (!m_ledger.isActiveTurn(requestId) || !info.usage)
        return;

    const auto &usage = *info.usage;

    LOG_MESSAGE(QString("Chat usage [%1]: prompt=%2 completion=%3 cached=%4 reasoning=%5")
                    .arg(requestId)
                    .arg(usage.promptTokens)
                    .arg(usage.completionTokens)
                    .arg(usage.cachedPromptTokens)
                    .arg(usage.reasoningTokens));

    emit sessionEvent(
        Session::UsageReported{
            .turnId = requestId,
            .usage = Session::Usage{
                .promptTokens = usage.promptTokens,
                .completionTokens = usage.completionTokens,
                .cachedPromptTokens = usage.cachedPromptTokens,
                .reasoningTokens = usage.reasoningTokens}});
}

void LlmChatBackend::handleFailed(const QString &requestId, const QString &error)
{
    if (!m_ledger.isActiveTurn(requestId))
        return;

    LOG_MESSAGE(QString("Chat request %1 failed: %2").arg(requestId, error));

    releaseRequest();

    emit sessionEvent(Session::TurnFailed{.turnId = requestId, .error = error});
}

void LlmChatBackend::handleThinkingBlock(
    const QString &requestId, const QString &thinking, const QString &signature)
{
    if (!m_ledger.isActiveTurn(requestId))
        return;

    emit sessionEvent(
        Session::ThinkingReceived{
            .turnId = requestId,
            .text = thinking,
            .signature = signature,
            .redacted = thinking.isEmpty()});
}

void LlmChatBackend::handleToolStarted(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QJsonObject &arguments)
{
    if (!m_ledger.isActiveTurn(requestId))
        return;

    emit sessionEvent(
        Session::ToolCallUpdated{
            .turnId = requestId,
            .toolId = toolId,
            .name = toolName,
            .status = QStringLiteral("in_progress"),
            .arguments = arguments,
            .dropPrecedingText = m_dropPreToolText});
}

void LlmChatBackend::handleToolResult(
    const QString &requestId,
    const QString &toolId,
    const QString &toolName,
    const QString &toolOutput)
{
    if (!m_ledger.isActiveTurn(requestId))
        return;

    const bool failed = toolOutput.startsWith(QLatin1String("Error: "));
    emit sessionEvent(
        Session::ToolCallUpdated{
            .turnId = requestId,
            .toolId = toolId,
            .name = toolName,
            .status = failed ? QStringLiteral("failed") : QStringLiteral("completed"),
            .result = toolOutput});
}

} // namespace QodeAssist::Chat
