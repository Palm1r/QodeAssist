// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatCompressor.hpp"

#include <memory>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>

#include "ChatModel.hpp"
#include "GeneralSettings.hpp"
#include "logger/Logger.hpp"

#include <ConversationHistory.hpp>
#include <Message.hpp>
#include <Session.hpp>
#include <SessionManager.hpp>
#include <SystemPromptBuilder.hpp>

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

void ChatCompressor::setSessionManager(SessionManager *sessionManager)
{
    m_sessionManager = sessionManager;
}

void ChatCompressor::setActiveAgent(const QString &agentName)
{
    m_activeAgent = agentName;
}

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

    if (!m_sessionManager) {
        emit compressionFailed(tr("Chat session manager is not available"));
        return;
    }

    QString sessionError;
    Session *session = m_sessionManager->createSession(m_activeAgent, &sessionError);
    if (!session) {
        emit compressionFailed(
            sessionError.isEmpty() ? tr("No chat agent selected") : sessionError);
        return;
    }

    auto *client = session->client();
    if (!client) {
        m_sessionManager->removeSession(session);
        emit compressionFailed(tr("Chat agent has no live client"));
        return;
    }

    m_isCompressing = true;
    m_chatModel = chatModel;
    m_originalChatPath = chatFilePath;
    m_accumulatedSummary.clear();
    m_session = session;

    emit compressionStarted();

    session->systemPrompt()->setLayer(
        QStringLiteral("compression"),
        QStringLiteral(
            "You are a helpful assistant that creates concise summaries of conversations. "
            "Your summaries preserve key information, technical details, and the flow of "
            "discussion."));

    auto *history = session->history();
    for (const auto &msg : m_chatModel->getChatHistory()) {
        if (msg.role == ChatModel::ChatRole::Tool || msg.role == ChatModel::ChatRole::FileEdit
            || msg.role == ChatModel::ChatRole::Thinking)
            continue;
        if (msg.content.trimmed().isEmpty())
            continue;

        Message apiMessage(
            msg.role == ChatModel::ChatRole::User ? Message::Role::User : Message::Role::Assistant);
        apiMessage.appendBlock(std::make_unique<LLMQore::TextContent>(msg.content));
        history->append(std::move(apiMessage));
    }

    m_connections.append(connect(
        client, &::LLMQore::BaseClient::chunkReceived,
        this, &ChatCompressor::onPartialResponseReceived, Qt::UniqueConnection));
    m_connections.append(connect(
        client, &::LLMQore::BaseClient::requestCompleted,
        this, &ChatCompressor::onFullResponseReceived, Qt::UniqueConnection));
    m_connections.append(connect(
        client, &::LLMQore::BaseClient::requestFailed,
        this, &ChatCompressor::onRequestFailed, Qt::UniqueConnection));

    client->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    blocks.push_back(std::make_unique<LLMQore::TextContent>(buildCompressionPrompt()));

    m_currentRequestId = session->send(
        std::move(blocks), /*toolsOverride=*/false, /*thinkingOverride=*/false);
    if (m_currentRequestId.isEmpty()) {
        handleCompressionError(tr("Failed to start compression request"));
        return;
    }
    LOG_MESSAGE(QString("Starting compression request: %1").arg(m_currentRequestId));
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

    const QString compressedPath = createCompressedChatPath(m_originalChatPath);
    const QString summary = m_accumulatedSummary;
    const QString sourcePath = m_originalChatPath;

    cleanupState();

    if (!createCompressedChatFile(sourcePath, compressedPath, summary)) {
        emit compressionFailed(tr("Failed to save compressed chat"));
        return;
    }

    LOG_MESSAGE(QString("Compression completed: %1").arg(compressedPath));
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
    root["compressedFrom"] = sourcePath;
    root["compressedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

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

void ChatCompressor::disconnectAllSignals()
{
    for (const auto &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
}

void ChatCompressor::cleanupState()
{
    disconnectAllSignals();

    Session *session = m_session;

    m_isCompressing = false;
    m_currentRequestId.clear();
    m_originalChatPath.clear();
    m_accumulatedSummary.clear();
    m_chatModel = nullptr;
    m_session = nullptr;

    if (session && m_sessionManager)
        m_sessionManager->removeSession(session);
}

} // namespace QodeAssist::Chat
