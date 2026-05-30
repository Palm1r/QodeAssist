// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "ChatCompressor.hpp"

#include <memory>

#include <LLMQore/BaseClient.hpp>
#include <LLMQore/ContentBlocks.hpp>

#include <projectexplorer/project.h>
#include <projectexplorer/projectmanager.h>

#include "GeneralSettings.hpp"
#include "logger/Logger.hpp"

#include <AgentFactory.hpp>
#include <ContextRenderer.hpp>
#include <ConversationHistory.hpp>
#include <Message.hpp>
#include <Session.hpp>
#include <SessionManager.hpp>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
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

void ChatCompressor::startCompression(
    const QString &chatFilePath, ConversationHistory *sourceHistory)
{
    if (m_isCompressing) {
        emit compressionFailed(tr("Compression already in progress"));
        return;
    }

    if (chatFilePath.isEmpty()) {
        emit compressionFailed(tr("No chat file to compress"));
        return;
    }

    if (!sourceHistory || sourceHistory->isEmpty()) {
        emit compressionFailed(tr("Chat is empty, nothing to compress"));
        return;
    }

    if (!m_sessionManager) {
        emit compressionFailed(tr("Chat session manager is not available"));
        return;
    }

    QString sessionError;
    Session *session = m_sessionManager->acquire(m_activeAgent, &sessionError);
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

    auto *project = ProjectExplorer::ProjectManager::startupProject();
    Templates::ContextRenderer::Bindings bindings;
    bindings.projectDir = project ? project->projectDirectory().toFSPathString() : QString();
    bindings.configDir = AgentFactory::userConfigDir();
    session->setContextBindings(bindings);

    m_isCompressing = true;
    m_originalChatPath = chatFilePath;
    m_session = session;

    emit compressionStarted();

    QStringList transcriptParts;
    for (const auto &msg : sourceHistory->messages()) {
        if (msg.role() != Message::Role::User && msg.role() != Message::Role::Assistant)
            continue;
        const QString text = msg.text();
        if (text.trimmed().isEmpty())
            continue;

        const QString role = msg.role() == Message::Role::User ? QStringLiteral("User")
                                                               : QStringLiteral("Assistant");
        transcriptParts.append(QStringLiteral("%1: %2").arg(role, text));
    }

    if (transcriptParts.isEmpty()) {
        handleCompressionError(tr("Chat is empty, nothing to compress"));
        return;
    }

    const QString transcript = transcriptParts.join(QStringLiteral("\n\n"));

    connect(
        session, &Session::finished, this,
        [this](const LLMQore::RequestID &id, const QString &) { onCompressionFinished(id); });
    connect(
        session, &Session::failed, this,
        [this](const LLMQore::RequestID &id, const QodeAssist::ErrorInfo &error) {
            onCompressionFailed(id, error.message);
        });

    client->setTransferTimeout(
        static_cast<int>(Settings::generalSettings().requestTimeout() * 1000));

    std::vector<std::unique_ptr<LLMQore::ContentBlock>> blocks;
    blocks.push_back(std::make_unique<LLMQore::TextContent>(transcript));

    m_currentRequestId = session->send(std::move(blocks));
    if (m_currentRequestId.isEmpty()) {
        handleCompressionError(tr("Failed to start compression request: %1")
                                   .arg(session->lastError().message));
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

void ChatCompressor::onCompressionFinished(const QString &requestId)
{
    if (!m_isCompressing || requestId != m_currentRequestId)
        return;

    QString summary;
    if (m_session) {
        if (auto *history = m_session->history(); history && !history->isEmpty())
            summary = history->messages().back().text();
    }

    LOG_MESSAGE(QString("Received summary, length: %1 characters").arg(summary.length()));

    const QString compressedPath = createCompressedChatPath(m_originalChatPath);
    const QString sourcePath = m_originalChatPath;

    cleanupState();

    if (!createCompressedChatFile(sourcePath, compressedPath, summary)) {
        emit compressionFailed(tr("Failed to save compressed chat"));
        return;
    }

    LOG_MESSAGE(QString("Compression completed: %1").arg(compressedPath));
    emit compressionCompleted(compressedPath);
}

void ChatCompressor::onCompressionFailed(const QString &requestId, const QString &error)
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
    summaryMessage["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject textBlock;
    textBlock["type"] = "text";
    textBlock["text"] = QString("# Chat Summary\n\n%1").arg(summary);
    summaryMessage["blocks"] = QJsonArray{textBlock};

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

void ChatCompressor::cleanupState()
{
    Session *session = m_session;

    m_isCompressing = false;
    m_currentRequestId.clear();
    m_originalChatPath.clear();
    m_session = nullptr;

    if (session && m_sessionManager)
        m_sessionManager->release(session);
}

} // namespace QodeAssist::Chat
