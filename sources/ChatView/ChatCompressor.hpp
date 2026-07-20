// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later
// Additional attribution terms under GPLv3 §7(b) apply — see LICENSE

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include "session/HistoryProjection.hpp"

namespace QodeAssist::Providers {
class Provider;
} // namespace QodeAssist::Providers

namespace QodeAssist::Templates {
class PromptTemplate;
} // namespace QodeAssist::Templates

namespace QodeAssist::Chat {

class ChatCompressor : public QObject
{
    Q_OBJECT

public:
    explicit ChatCompressor(QObject *parent = nullptr);

    void startCompression(
        const QString &chatFilePath, const Session::ConversationHistory &history);
    void startSummary(const Session::ConversationHistory &history);

signals:
    void compressingChanged();

public:

    bool isCompressing() const;
    void cancelCompression();

    static QString configurationIssue();

signals:
    void compressionStarted();
    void compressionCompleted(const QString &compressedChatPath);
    void summaryReady(const QString &summary);
    void compressionFailed(const QString &error);

private slots:
    void onPartialResponseReceived(const QString &requestId, const QString &partialText);
    void onFullResponseReceived(const QString &requestId, const QString &fullText);
    void onRequestFailed(const QString &requestId, const QString &error);

private:
    QString createCompressedChatPath(const QString &originalPath) const;
    QString buildCompressionPrompt() const;
    bool createCompressedChatFile(
        const QString &sourcePath, const QString &destPath, const QString &summary);
    void connectProviderSignals();
    void disconnectAllSignals();
    void cleanupState();
    void handleCompressionError(const QString &error);
    void buildRequestPayload(QJsonObject &payload, Templates::PromptTemplate *promptTemplate);
    void beginCompression(
        const QString &chatFilePath,
        const Session::ConversationHistory &history,
        bool summaryOnly);

    bool m_isCompressing = false;
    bool m_summaryOnly = false;
    QString m_currentRequestId;
    QString m_originalChatPath;
    QString m_accumulatedSummary;
    Providers::Provider *m_provider = nullptr;
    QList<Session::MessageRow> m_rows;

    QList<QMetaObject::Connection> m_connections;
};

} // namespace QodeAssist::Chat
