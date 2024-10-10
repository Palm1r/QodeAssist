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

#include "RequestHandler.hpp"

namespace QodeAssist::Chat {

struct CMessage
{
    enum class Role { System, User, Assistant };
    Role role;
    QString content;
    int tokenCount;
};

class CHistory
{
public:
    void addMessage(CMessage::Role role, const QString &content);
    void clear();
    QVector<CMessage> getMessages() const;
    QString getSystemPrompt() const;
    void setSystemPrompt(const QString &prompt);
    void trim();

private:
    QVector<CMessage> m_messages;
    QString m_systemPrompt;
    int m_totalTokens = 0;
    static const int MAX_HISTORY_SIZE = 50;
    static const int MAX_TOKENS = 4000;

    int estimateTokenCount(const QString &text) const;
};

class ClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit ClientInterface(QObject *parent = nullptr);
    ~ClientInterface();

    void sendMessage(const QString &message);
    void clearMessages();
    QVector<CMessage> getChatHistory() const;

signals:
    void messageReceived(const QString &message);
    void errorOccurred(const QString &error);

private:
    void handleLLMResponse(const QString &response, bool isComplete);
    QJsonArray prepareMessagesForRequest() const;

    LLMCore::RequestHandler *m_requestHandler;
    QString m_accumulatedResponse;
    CHistory m_chatHistory;
};

} // namespace QodeAssist::Chat
