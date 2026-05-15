// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#include "InputTokenCounter.hpp"

#include <algorithm>

#include <LLMQore/ToolsManager.hpp>
#include <QJsonArray>
#include <QJsonDocument>

#include <utils/aspects.h>

#include "ChatAssistantSettings.hpp"
#include "ChatModel.hpp"
#include "GeneralSettings.hpp"
#include "Logger.hpp"
#include "ProvidersManager.hpp"
#include "context/ContextManager.hpp"
#include "context/TokenUtils.hpp"

namespace QodeAssist::Chat {

InputTokenCounter::InputTokenCounter(
    ChatModel *chatModel, Context::ContextManager *contextManager, QObject *parent)
    : QObject(parent)
    , m_chatModel(chatModel)
    , m_contextManager(contextManager)
{
    auto &settings = Settings::chatAssistantSettings();
    connect(
        &settings.useSystemPrompt,
        &Utils::BaseAspect::changed,
        this,
        &InputTokenCounter::recompute);
    connect(
        &settings.systemPrompt, &Utils::BaseAspect::changed, this, &InputTokenCounter::recompute);
    connect(
        &settings.enableChatTools,
        &Utils::BaseAspect::changed,
        this,
        &InputTokenCounter::recompute);

    connect(&Settings::generalSettings().caProvider, &Utils::BaseAspect::changed, this, [this]() {
        rewireToolsChangedConnection();
        recompute();
    });

    rewireToolsChangedConnection();
    recompute();
}

int InputTokenCounter::inputTokens() const
{
    return m_inputTokens;
}

void InputTokenCounter::setMessage(const QString &message)
{
    m_messageTokens = Context::TokenUtils::estimateTokens(message);
    recompute();
}

void InputTokenCounter::setAttachments(const QStringList &attachments)
{
    m_attachments = attachments;
    recompute();
}

void InputTokenCounter::setLinkedFiles(const QStringList &linkedFiles)
{
    m_linkedFiles = linkedFiles;
    recompute();
}

void InputTokenCounter::rewireToolsChangedConnection()
{
    if (m_toolsChangedConn)
        QObject::disconnect(m_toolsChangedConn);
    m_toolsChangedConn = {};

    const auto providerName = Settings::generalSettings().caProvider();
    auto *provider = PluginLLMCore::ProvidersManager::instance().getProviderByName(providerName);
    if (!provider)
        return;
    auto *tm = provider->toolsManager();
    if (!tm)
        return;

    m_toolsChangedConn = connect(
        tm, &::LLMQore::ToolRegistry::toolsChanged, this, &InputTokenCounter::recompute);
}

void InputTokenCounter::recompute()
{
    int inputTokens = m_messageTokens;
    auto &settings = Settings::chatAssistantSettings();

    if (settings.useSystemPrompt()) {
        inputTokens += Context::TokenUtils::estimateTokens(settings.systemPrompt());
    }

    const auto splitImageEstimate = [](const QStringList &paths, QStringList &textPaths) {
        int imageTokens = 0;
        for (const QString &p : paths) {
            if (Context::TokenUtils::isImageFilePath(p))
                imageTokens += Context::TokenUtils::estimateImageAttachmentTokens(p);
            else
                textPaths.append(p);
        }
        return imageTokens;
    };

    if (!m_attachments.isEmpty()) {
        QStringList textPaths;
        inputTokens += splitImageEstimate(m_attachments, textPaths);
        if (!textPaths.isEmpty()) {
            auto attachFiles = m_contextManager->getContentFiles(textPaths);
            inputTokens += Context::TokenUtils::estimateFilesTokens(attachFiles);
        }
    }

    if (!m_linkedFiles.isEmpty()) {
        QStringList textPaths;
        inputTokens += splitImageEstimate(m_linkedFiles, textPaths);
        if (!textPaths.isEmpty()) {
            auto linkFiles = m_contextManager->getContentFiles(textPaths);
            inputTokens += Context::TokenUtils::estimateFilesTokens(linkFiles);
        }
    }

    const auto &history = m_chatModel->getChatHistory();
    for (const auto &message : history) {
        inputTokens += Context::TokenUtils::estimateTokens(message.content);
        inputTokens += 4; // + role
    }

    if (settings.enableChatTools()) {
        const auto providerName = Settings::generalSettings().caProvider();
        if (auto *provider = PluginLLMCore::ProvidersManager::instance().getProviderByName(
                providerName)) {
            if (auto *tm = provider->toolsManager()) {
                const QJsonArray toolDefs = tm->getToolsDefinitions();
                if (!toolDefs.isEmpty()) {
                    const QByteArray serialized
                        = QJsonDocument(toolDefs).toJson(QJsonDocument::Compact);
                    inputTokens += static_cast<int>(serialized.size() / 4);
                }
            }
        }
    }

    m_inputTokens = static_cast<int>(inputTokens * m_calibrationFactor);
    emit inputTokensChanged();
}

void InputTokenCounter::recordSent()
{
    m_lastSentEstimate = m_calibrationFactor > 0.0
                             ? static_cast<int>(m_inputTokens / m_calibrationFactor)
                             : m_inputTokens;
}

void InputTokenCounter::recordServerUsage(int promptTokens)
{
    if (promptTokens <= 0 || m_lastSentEstimate <= 0)
        return;

    const double rawFactor
        = static_cast<double>(promptTokens) / static_cast<double>(m_lastSentEstimate);
    const double clamped = std::clamp(rawFactor, 0.5, 3.0);
    m_calibrationFactor = 0.5 * m_calibrationFactor + 0.5 * clamped;

    LOG_MESSAGE(QString("Token calibration: server=%1 estimated=%2 ratio=%3 ema=%4")
                    .arg(promptTokens)
                    .arg(m_lastSentEstimate)
                    .arg(rawFactor, 0, 'f', 3)
                    .arg(m_calibrationFactor, 0, 'f', 3));

    recompute();
}

} // namespace QodeAssist::Chat
