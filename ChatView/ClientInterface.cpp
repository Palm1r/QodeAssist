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

#include "ClientInterface.hpp"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/target.h>
#include <texteditor/textdocument.h>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QUuid>

#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>
#include <coreplugin/idocument.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/projectmanager.h>

#include <texteditor/textdocument.h>
#include <texteditor/texteditor.h>

#include "ChatAssistantSettings.hpp"
#include "ChatSerializer.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "ProvidersManager.hpp"
#include "RequestConfig.hpp"
#include "ToolsSettings.hpp"
#include <RulesLoader.hpp>
#include <context/ChangesManager.h>

namespace QodeAssist::Chat {

ClientInterface::ClientInterface(
    ChatModel *chatModel, LLMCore::IPromptProvider *promptProvider, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
    , m_promptProvider(promptProvider)
    , m_contextManager(new Context::ContextManager(this))
{}

ClientInterface::~ClientInterface()
{
    cancelRequest();
}

void ClientInterface::sendMessage(
    const QString &message,
    const QList<QString> &attachments,
    const QList<QString> &linkedFiles,
    bool useTools,
    bool useThinking)
{
    cancelRequest();
    m_accumulatedResponses.clear();

    Context::ChangesManager::instance().archiveAllNonArchivedEdits();

    QList<QString> imageFiles;
    QList<QString> textFiles;

    for (const QString &filePath : attachments) {
        if (isImageFile(filePath)) {
            imageFiles.append(filePath);
        } else {
            textFiles.append(filePath);
        }
    }

    auto attachFiles = m_contextManager->getContentFiles(textFiles);

    QList<ChatModel::ImageAttachment> imageAttachments;
    if (!imageFiles.isEmpty() && !m_chatFilePath.isEmpty()) {
        for (const QString &imagePath : imageFiles) {
            QString base64Data = encodeImageToBase64(imagePath);
            if (base64Data.isEmpty()) {
                continue;
            }

            QString storedPath;
            QFileInfo fileInfo(imagePath);
            if (ChatSerializer::saveImageToStorage(
                    m_chatFilePath, fileInfo.fileName(), base64Data, storedPath)) {
                ChatModel::ImageAttachment imageAttachment;
                imageAttachment.fileName = fileInfo.fileName();
                imageAttachment.storedPath = storedPath;
                imageAttachment.mediaType = getMediaTypeForImage(imagePath);
                imageAttachments.append(imageAttachment);

                LOG_MESSAGE(QString("Stored image %1 as %2").arg(fileInfo.fileName(), storedPath));
            }
        }
    } else if (!imageFiles.isEmpty()) {
        LOG_MESSAGE(QString("Warning: Chat file path not set, cannot save %1 image(s)")
                        .arg(imageFiles.size()));
    }

    m_chatModel->addMessage(message, ChatModel::ChatRole::User, "", attachFiles, imageAttachments);

    auto &chatAssistantSettings = Settings::chatAssistantSettings();

    auto providerName = Settings::generalSettings().caProvider();
    auto provider = LLMCore::ProvidersManager::instance().getProviderByName(providerName);

    if (!provider) {
        LOG_MESSAGE(QString("No provider found with name: %1").arg(providerName));
        return;
    }

    auto templateName = Settings::generalSettings().caTemplate();
    auto promptTemplate = m_promptProvider->getTemplateByName(templateName);

    if (!promptTemplate) {
        LOG_MESSAGE(QString("No template found with name: %1").arg(templateName));
        return;
    }

    LLMCore::ContextData context;

    const bool isToolsEnabled = useTools;

    if (chatAssistantSettings.useSystemPrompt()) {
        QString systemPrompt = chatAssistantSettings.systemPrompt();

        auto project = LLMCore::RulesLoader::getActiveProject();

        if (project) {
            systemPrompt += QString("\n# Active project name: %1").arg(project->displayName());
            systemPrompt += QString("\n# Active Project path: %1")
                                .arg(project->projectDirectory().toUrlishString());

            if (auto target = project->activeTarget()) {
                if (auto buildConfig = target->activeBuildConfiguration()) {
                    systemPrompt += QString("\n# Active Build directory: %1")
                                        .arg(buildConfig->buildDirectory().toUrlishString());
                }
            }

            QString projectRules
                = LLMCore::RulesLoader::loadRulesForProject(project, LLMCore::RulesContext::Chat);

            if (!projectRules.isEmpty()) {
                systemPrompt += QString("\n# Project Rules\n\n") + projectRules;
            }
        } else {
            systemPrompt += QString("\n# No active project in IDE");
        }

        if (!linkedFiles.isEmpty()) {
            systemPrompt = getSystemPromptWithLinkedFiles(systemPrompt, linkedFiles);
        }
        context.systemPrompt = systemPrompt;
    }

    QVector<LLMCore::Message> messages;
    for (const auto &msg : m_chatModel->getChatHistory()) {
        if (msg.role == ChatModel::ChatRole::Tool || msg.role == ChatModel::ChatRole::FileEdit) {
            continue;
        }

        LLMCore::Message apiMessage;
        apiMessage.role = msg.role == ChatModel::ChatRole::User ? "user" : "assistant";
        apiMessage.content = msg.content;
        apiMessage.isThinking = (msg.role == ChatModel::ChatRole::Thinking);
        apiMessage.isRedacted = msg.isRedacted;
        apiMessage.signature = msg.signature;

        if (provider->supportImage() && !m_chatFilePath.isEmpty() && !msg.images.isEmpty()) {
            auto apiImages = loadImagesFromStorage(msg.images);
            if (!apiImages.isEmpty()) {
                apiMessage.images = apiImages;
            }
        }

        messages.append(apiMessage);
    }

    if (!imageFiles.isEmpty() && !provider->supportImage()) {
        LOG_MESSAGE(QString("Provider %1 doesn't support images, %2 ignored")
                        .arg(provider->name(), QString::number(imageFiles.size())));
    }

    context.history = messages;

    LLMCore::LLMConfig config;
    config.requestType = LLMCore::RequestType::Chat;
    config.provider = provider;
    config.promptTemplate = promptTemplate;
    if (provider->providerID() == LLMCore::ProviderID::GoogleAI) {
        QString stream = QString{"streamGenerateContent?alt=sse"};
        config.url = QUrl(QString("%1/models/%2:%3")
                              .arg(
                                  Settings::generalSettings().caUrl(),
                                  Settings::generalSettings().caModel(),
                                  stream));
    } else {
        config.url
            = QString("%1%2").arg(Settings::generalSettings().caUrl(), provider->chatEndpoint());
        config.providerRequest
            = {{"model", Settings::generalSettings().caModel()}, {"stream", true}};
    }

    config.apiKey = provider->apiKey();

    config.provider->prepareRequest(
        config.providerRequest,
        promptTemplate,
        context,
        LLMCore::RequestType::Chat,
        useTools,
        useThinking);

    QString requestId = QUuid::createUuid().toString();
    QJsonObject request{{"id", requestId}};

    m_activeRequests[requestId] = {request, provider};

    emit requestStarted(requestId);

    connect(
        provider,
        &LLMCore::Provider::partialResponseReceived,
        this,
        &ClientInterface::handlePartialResponse,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::fullResponseReceived,
        this,
        &ClientInterface::handleFullResponse,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::requestFailed,
        this,
        &ClientInterface::handleRequestFailed,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::toolExecutionStarted,
        m_chatModel,
        &ChatModel::addToolExecutionStatus,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::toolExecutionCompleted,
        m_chatModel,
        &ChatModel::updateToolResult,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::continuationStarted,
        this,
        &ClientInterface::handleCleanAccumulatedData,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::thinkingBlockReceived,
        m_chatModel,
        &ChatModel::addThinkingBlock,
        Qt::UniqueConnection);
    connect(
        provider,
        &LLMCore::Provider::redactedThinkingBlockReceived,
        m_chatModel,
        &ChatModel::addRedactedThinkingBlock,
        Qt::UniqueConnection);

    provider->sendRequest(requestId, config.url, config.providerRequest);
}

void ClientInterface::clearMessages()
{
    m_chatModel->clear();
    LOG_MESSAGE("Chat history cleared");
}

void ClientInterface::cancelRequest()
{
    QSet<LLMCore::Provider *> providers;
    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        if (it.value().provider) {
            providers.insert(it.value().provider);
        }
    }

    for (auto *provider : providers) {
        disconnect(provider, nullptr, this, nullptr);
    }

    for (auto it = m_activeRequests.begin(); it != m_activeRequests.end(); ++it) {
        const RequestContext &ctx = it.value();
        if (ctx.provider) {
            ctx.provider->cancelRequest(it.key());
        }
    }

    m_activeRequests.clear();
    m_accumulatedResponses.clear();

    LOG_MESSAGE("All requests cancelled and state cleared");
}

void ClientInterface::handleLLMResponse(const QString &response, const QJsonObject &request)
{
    const auto message = response.trimmed();

    if (!message.isEmpty()) {
        QString messageId = request["id"].toString();
        m_chatModel->addMessage(message, ChatModel::ChatRole::Assistant, messageId);
    }
}

QString ClientInterface::getCurrentFileContext() const
{
    auto currentEditor = Core::EditorManager::currentEditor();
    if (!currentEditor) {
        LOG_MESSAGE("No active editor found");
        return QString();
    }

    auto textDocument = qobject_cast<TextEditor::TextDocument *>(currentEditor->document());
    if (!textDocument) {
        LOG_MESSAGE("Current document is not a text document");
        return QString();
    }

    QString fileInfo = QString("Language: %1\nFile: %2\n\n")
                           .arg(textDocument->mimeType(), textDocument->filePath().toFSPathString());

    QString content = textDocument->document()->toPlainText();

    LOG_MESSAGE(QString("Got context from file: %1").arg(textDocument->filePath().toFSPathString()));

    return QString("Current file context:\n%1\nFile content:\n%2").arg(fileInfo, content);
}

QString ClientInterface::getSystemPromptWithLinkedFiles(
    const QString &basePrompt, const QList<QString> &linkedFiles) const
{
    QString updatedPrompt = basePrompt;

    if (!linkedFiles.isEmpty()) {
        updatedPrompt += "\n\nLinked files for reference:\n";

        auto contentFiles = m_contextManager->getContentFiles(linkedFiles);
        for (const auto &file : contentFiles) {
            updatedPrompt += QString("\nFile: %1\nContent:\n%2\n").arg(file.filename, file.content);
        }
    }

    return updatedPrompt;
}

Context::ContextManager *ClientInterface::contextManager() const
{
    return m_contextManager;
}

void ClientInterface::handlePartialResponse(const QString &requestId, const QString &partialText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    m_accumulatedResponses[requestId] += partialText;

    const RequestContext &ctx = it.value();
    handleLLMResponse(m_accumulatedResponses[requestId], ctx.originalRequest);
}

void ClientInterface::handleFullResponse(const QString &requestId, const QString &fullText)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    const RequestContext &ctx = it.value();

    QString finalText = !fullText.isEmpty() ? fullText : m_accumulatedResponses[requestId];

    QString applyError;
    bool applySuccess
        = Context::ChangesManager::instance().applyPendingEditsForRequest(requestId, &applyError);

    if (!applySuccess) {
        LOG_MESSAGE(QString("Some edits for request %1 were not auto-applied: %2")
                        .arg(requestId, applyError));
    }

    LOG_MESSAGE(
        "Message completed. Final response for message " + ctx.originalRequest["id"].toString()
        + ": " + finalText);
    emit messageReceivedCompletely();

    if (it != m_activeRequests.end()) {
        m_activeRequests.erase(it);
    }
    if (m_accumulatedResponses.contains(requestId)) {
        m_accumulatedResponses.remove(requestId);
    }
}

void ClientInterface::handleRequestFailed(const QString &requestId, const QString &error)
{
    auto it = m_activeRequests.find(requestId);
    if (it == m_activeRequests.end())
        return;

    LOG_MESSAGE(QString("Chat request %1 failed: %2").arg(requestId, error));
    emit errorOccurred(error);

    if (it != m_activeRequests.end()) {
        m_activeRequests.erase(it);
    }
    if (m_accumulatedResponses.contains(requestId)) {
        m_accumulatedResponses.remove(requestId);
    }
}

void ClientInterface::handleCleanAccumulatedData(const QString &requestId)
{
    m_accumulatedResponses[requestId].clear();
    LOG_MESSAGE(QString("Cleared accumulated responses for continuation request %1").arg(requestId));
}

bool ClientInterface::isImageFile(const QString &filePath) const
{
    static const QSet<QString> imageExtensions = {"png", "jpg", "jpeg", "gif", "webp", "bmp", "svg"};

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    return imageExtensions.contains(extension);
}

QString ClientInterface::getMediaTypeForImage(const QString &filePath) const
{
    static const QHash<QString, QString> mediaTypes
        = {{"png", "image/png"},
           {"jpg", "image/jpeg"},
           {"jpeg", "image/jpeg"},
           {"gif", "image/gif"},
           {"webp", "image/webp"},
           {"bmp", "image/bmp"},
           {"svg", "image/svg+xml"}};

    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();

    if (mediaTypes.contains(extension)) {
        return mediaTypes[extension];
    }

    QMimeDatabase mimeDb;
    QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
    return mimeType.name();
}

QString ClientInterface::encodeImageToBase64(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open image file: %1").arg(filePath));
        return QString();
    }

    QByteArray imageData = file.readAll();
    file.close();

    return imageData.toBase64();
}

QVector<LLMCore::ImageAttachment> ClientInterface::loadImagesFromStorage(
    const QList<ChatModel::ImageAttachment> &storedImages) const
{
    QVector<LLMCore::ImageAttachment> apiImages;

    for (const auto &storedImage : storedImages) {
        QString base64Data
            = ChatSerializer::loadImageFromStorage(m_chatFilePath, storedImage.storedPath);
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

void ClientInterface::setChatFilePath(const QString &filePath)
{
    m_chatFilePath = filePath;
    m_chatModel->setChatFilePath(filePath);
}

QString ClientInterface::chatFilePath() const
{
    return m_chatFilePath;
}

} // namespace QodeAssist::Chat
