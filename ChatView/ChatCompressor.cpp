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

#include "ChatCompressor.hpp"
#include "ChatModel.hpp"
#include "GeneralSettings.hpp"
#include "PromptTemplateManager.hpp"
#include "ProvidersManager.hpp"
#include "logger/Logger.hpp"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace QodeAssist::Chat {

ChatCompressor::ChatCompressor(QObject *parent)
    : QObject(parent)
{}

void ChatCompressor::startCompression(const QString &chatFilePath, ChatModel *chatModel)
{
    if (m_isCompressing) {
        emit compressionFailed(tr("Compression already in progress"));
        return;
    }

    if (chatFilePath.isEmpty()) {
        emit compressionFailed(tr("No chat file to compress"));
        return;
    }

    if (!chatModel || chatModel->rowCount() == 0) {
        emit compressionFailed(tr("Chat is empty, nothing to compress"));
        return;
    }

    auto providerName = Settings::generalSettings().caProvider();
    m_provider = LLMCore::ProvidersManager::instance().getProviderByName(providerName);

    if (!m_provider) {
        emit compressionFailed(tr("No provider available"));
        return;
    }

    auto templateName = Settings::generalSettings().caTemplate();
    auto promptTemplate = LLMCore::PromptTemplateManager::instance().getChatTemplateByName(
        templateName);

    if (!promptTemplate) {
        emit compressionFailed(tr("No template available"));
        return;
    }

    m_isCompressing = true;
    m_chatModel = chatModel;
    m_originalChatPath = chatFilePath;
    m_accumulatedSummary.clear();
    m_currentRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    emit compressionStarted();

    connectProviderSignals();

    QUrl requestUrl;
    QJsonObject payload;

    if (m_provider->providerID() == LLMCore::ProviderID::GoogleAI) {
        requestUrl = QUrl(QString("%1/models/%2:streamGenerateContent?alt=sse")
                              .arg(Settings::generalSettings().caUrl(),
                                   Settings::generalSettings().caModel()));
    } else {
        requestUrl = QUrl(QString("%1%2").arg(Settings::generalSettings().caUrl(),
                                              m_provider->chatEndpoint()));
        payload["model"] = Settings::generalSettings().caModel();
        payload["stream"] = true;
    }

    buildRequestPayload(payload, promptTemplate);

    LOG_MESSAGE(QString("Starting compression request: %1").arg(m_currentRequestId));
    m_provider->sendRequest(m_currentRequestId, requestUrl, payload);
}

bool ChatCompressor::isCompressing() const
{
    return m_isCompressing;
}

void ChatCompressor::cancelCompression()
{
    if (!m_isCompressing)
        return;

    LOG_MESSAGE("Cancelling compression request");

    if (m_provider && !m_currentRequestId.isEmpty())
        m_provider->cancelRequest(m_currentRequestId);

    cleanupState();
    emit compressionFailed(tr("Compression cancelled"));
}

void ChatCompressor::onPartialResponseReceived(const QString &requestId, const QString &partialText)
{
    if (!m_isCompressing || requestId != m_currentRequestId)
        return;

    m_accumulatedSummary += partialText;
}

void ChatCompressor::onFullResponseReceived(const QString &requestId, const QString &fullText)
{
    Q_UNUSED(fullText)

    if (!m_isCompressing || requestId != m_currentRequestId)
        return;

    LOG_MESSAGE(
        QString("Received summary, length: %1 characters").arg(m_accumulatedSummary.length()));

    QString compressedPath = createCompressedChatPath(m_originalChatPath);
    if (!createCompressedChatFile(m_originalChatPath, compressedPath, m_accumulatedSummary)) {
        handleCompressionError(tr("Failed to save compressed chat"));
        return;
    }

    LOG_MESSAGE(QString("Compression completed: %1").arg(compressedPath));
    cleanupState();
    emit compressionCompleted(compressedPath);
}

void ChatCompressor::onRequestFailed(const QString &requestId, const QString &error)
{
    if (!m_isCompressing || requestId != m_currentRequestId)
        return;

    LOG_MESSAGE(QString("Compression request failed: %1").arg(error));
    handleCompressionError(tr("Compression failed: %1").arg(error));
}

void ChatCompressor::handleCompressionError(const QString &error)
{
    cleanupState();
    emit compressionFailed(error);
}

QString ChatCompressor::createCompressedChatPath(const QString &originalPath) const
{
    QFileInfo fileInfo(originalPath);
    QString hash = QString::number(QDateTime::currentMSecsSinceEpoch() % 100000, 16);
    return QString("%1/%2_%3.%4")
        .arg(fileInfo.absolutePath(), fileInfo.completeBaseName(), hash, fileInfo.suffix());
}

QString ChatCompressor::buildCompressionPrompt() const
{
    return QStringLiteral(
        "Please create a comprehensive summary of our entire conversation above. "
        "The summary should:\n"
        "1. Preserve all important context, decisions, and key information\n"
        "2. Maintain technical details, code snippets, file references, and specific examples\n"
        "3. Keep the chronological flow of the discussion\n"
        "4. Be significantly shorter than the original (aim for 30-40% of original length)\n"
        "5. Be written in clear, structured format\n"
        "6. Use markdown formatting for better readability\n\n"
        "Create the summary now:");
}

void ChatCompressor::buildRequestPayload(
    QJsonObject &payload, LLMCore::PromptTemplate *promptTemplate)
{
    LLMCore::ContextData context;

    context.systemPrompt = QStringLiteral(
        "You are a helpful assistant that creates concise summaries of conversations. "
        "Your summaries preserve key information, technical details, and the flow of discussion.");

    QVector<LLMCore::Message> messages;
    for (const auto &msg : m_chatModel->getChatHistory()) {
        if (msg.role == ChatModel::ChatRole::Tool 
            || msg.role == ChatModel::ChatRole::FileEdit
            || msg.role == ChatModel::ChatRole::Thinking)
            continue;

        LLMCore::Message apiMessage;
        apiMessage.role = (msg.role == ChatModel::ChatRole::User) ? "user" : "assistant";
        apiMessage.content = msg.content;
        messages.append(apiMessage);
    }

    LLMCore::Message compressionRequest;
    compressionRequest.role = "user";
    compressionRequest.content = buildCompressionPrompt();
    messages.append(compressionRequest);

    context.history = messages;

    m_provider->prepareRequest(
        payload, promptTemplate, context, LLMCore::RequestType::Chat, false, false);
}

bool ChatCompressor::createCompressedChatFile(
    const QString &sourcePath, const QString &destPath, const QString &summary)
{
    QFile sourceFile(sourcePath);
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        LOG_MESSAGE(QString("Failed to open source chat file: %1").arg(sourcePath));
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(sourceFile.readAll(), &parseError);
    sourceFile.close();

    if (doc.isNull() || !doc.isObject()) {
        LOG_MESSAGE(QString("Invalid JSON in chat file: %1 (Error: %2)")
                        .arg(sourcePath, parseError.errorString()));
        return false;
    }

    QJsonObject root = doc.object();

    QJsonObject summaryMessage;
    summaryMessage["role"] = "assistant";
    summaryMessage["content"] = QString("# Chat Summary\n\n%1").arg(summary);
    summaryMessage["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    summaryMessage["isRedacted"] = false;
    summaryMessage["attachments"] = QJsonArray();
    summaryMessage["images"] = QJsonArray();

    root["messages"] = QJsonArray{summaryMessage};

    if (QFile::exists(destPath))
        QFile::remove(destPath);

    QFile destFile(destPath);
    if (!destFile.open(QIODevice::WriteOnly)) {
        LOG_MESSAGE(QString("Failed to create compressed chat file: %1").arg(destPath));
        return false;
    }

    destFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

void ChatCompressor::connectProviderSignals()
{
    m_connections.append(connect(
        m_provider,
        &LLMCore::Provider::partialResponseReceived,
        this,
        &ChatCompressor::onPartialResponseReceived,
        Qt::UniqueConnection));

    m_connections.append(connect(
        m_provider,
        &LLMCore::Provider::fullResponseReceived,
        this,
        &ChatCompressor::onFullResponseReceived,
        Qt::UniqueConnection));

    m_connections.append(connect(
        m_provider,
        &LLMCore::Provider::requestFailed,
        this,
        &ChatCompressor::onRequestFailed,
        Qt::UniqueConnection));
}

void ChatCompressor::disconnectAllSignals()
{
    for (const auto &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
}

void ChatCompressor::cleanupState()
{
    disconnectAllSignals();

    m_isCompressing = false;
    m_currentRequestId.clear();
    m_originalChatPath.clear();
    m_accumulatedSummary.clear();
    m_chatModel = nullptr;
    m_provider = nullptr;
}

} // namespace QodeAssist::Chat
