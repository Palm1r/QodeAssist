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

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace QodeAssist::LLMCore {
class Provider;
class PromptTemplate;
} // namespace QodeAssist::LLMCore

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
    void buildRequestPayload(QJsonObject &payload, LLMCore::PromptTemplate *promptTemplate);

    bool m_isCompressing = false;
    QString m_currentRequestId;
    QString m_originalChatPath;
    QString m_accumulatedSummary;
    LLMCore::Provider *m_provider = nullptr;
    ChatModel *m_chatModel = nullptr;

    QList<QMetaObject::Connection> m_connections;
};

} // namespace QodeAssist::Chat
