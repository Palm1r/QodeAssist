/* 
 * Copyright (C) 2024 Petr Mironychev
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

#include <QObject>
#include <QString>
#include <QVector>
#include "QodeAssistData.hpp"
#include "core/LLMRequestHandler.hpp"

namespace QodeAssist::Chat {

struct ChatMessage
{
    enum class Role { System, User, Assistant };
    Role role;
    QString content;
    int tokenCount;
};

class ChatHistory
{
public:
    void addMessage(ChatMessage::Role role, const QString &content);
    void clear();
    QVector<ChatMessage> getMessages() const;
    QString getSystemPrompt() const;
    void setSystemPrompt(const QString &prompt);
    void trim();

private:
    QVector<ChatMessage> m_messages;
    QString m_systemPrompt;
    int m_totalTokens = 0;
    static const int MAX_HISTORY_SIZE = 50;
    static const int MAX_TOKENS = 4000;

    int estimateTokenCount(const QString &text) const;
};

class ChatClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ChatClientInterface(QObject *parent = nullptr);
    ~ChatClientInterface();

    void sendMessage(const QString &message);
    void clearMessages();
    QVector<ChatMessage> getChatHistory() const;

signals:
    void messageReceived(const QString &message);
    void errorOccurred(const QString &error);

private:
    void handleLLMResponse(const QString &response, bool isComplete);
    QJsonArray prepareMessagesForRequest() const;

    LLMRequestHandler *m_requestHandler;
    QString m_accumulatedResponse;
    ChatHistory m_chatHistory;
};

} // namespace QodeAssist::Chat
