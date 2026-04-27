// Copyright (C) 2024-2026 Petr Mironychev
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace QodeAssist::PluginLLMCore {
class Provider;
class PromptTemplate;
} // namespace QodeAssist::PluginLLMCore

namespace QodeAssist::Chat {

class ChatModel;

class ChatCompressor : public QObject
{
    Q_OBJECT

public:
    explicit ChatCompressor(QObject *parent = nullptr);

    void startCompression(const QString &chatFilePath, ChatModel *chatModel);

    bool isCompressing() const;
    void cancelCompression();

signals:
    void compressionStarted();
    void compressionCompleted(const QString &compressedChatPath);
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
    void buildRequestPayload(QJsonObject &payload, PluginLLMCore::PromptTemplate *promptTemplate);

    bool m_isCompressing = false;
    QString m_currentRequestId;
    QString m_originalChatPath;
    QString m_accumulatedSummary;
    PluginLLMCore::Provider *m_provider = nullptr;
    ChatModel *m_chatModel = nullptr;

    QList<QMetaObject::Connection> m_connections;
};

} // namespace QodeAssist::Chat
