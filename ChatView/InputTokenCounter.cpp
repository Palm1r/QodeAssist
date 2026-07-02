// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#include "InputTokenCounter.hpp"

#include "context/ContextManager.hpp"
#include "context/TokenUtils.hpp"

#include <ConversationHistory.hpp>
#include <Message.hpp>

namespace QodeAssist::Chat {

InputTokenCounter::InputTokenCounter(
    ConversationHistory *history, Context::ContextManager *contextManager, QObject *parent)
    : QObject(parent)
    , m_history(history)
    , m_contextManager(contextManager)
{
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

void InputTokenCounter::recompute()
{
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

    int pendingTokens = m_messageTokens;
    if (!m_attachments.isEmpty()) {
        QStringList textPaths;
        pendingTokens += splitImageEstimate(m_attachments, textPaths);
        if (!textPaths.isEmpty()) {
            auto attachFiles = m_contextManager->getContentFiles(textPaths);
            pendingTokens += Context::TokenUtils::estimateFilesTokens(attachFiles);
        }
    }

    if (m_hasServerUsage && m_history && !m_history->isEmpty()) {
        m_inputTokens = m_serverInputTokens + pendingTokens;
        emit inputTokensChanged();
        return;
    }

    int inputTokens = pendingTokens;

    if (!m_linkedFiles.isEmpty()) {
        QStringList textPaths;
        inputTokens += splitImageEstimate(m_linkedFiles, textPaths);
        if (!textPaths.isEmpty()) {
            auto linkFiles = m_contextManager->getContentFiles(textPaths);
            inputTokens += Context::TokenUtils::estimateFilesTokens(linkFiles);
        }
    }

    if (m_history) {
        for (const auto &message : m_history->messages()) {
            inputTokens += Context::TokenUtils::estimateTokens(message.text());
            inputTokens += 4;
        }
    }

    m_inputTokens = inputTokens;
    emit inputTokensChanged();
}

void InputTokenCounter::recordServerUsage(int promptTokens, int cachedTokens)
{
    const int serverInput = promptTokens + cachedTokens;
    if (serverInput <= 0)
        return;

    m_serverInputTokens = serverInput;
    m_hasServerUsage = true;
    recompute();
}

void InputTokenCounter::resetServerUsage()
{
    m_serverInputTokens = 0;
    m_hasServerUsage = false;
    recompute();
}

} // namespace QodeAssist::Chat
