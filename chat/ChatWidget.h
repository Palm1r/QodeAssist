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

#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

#include "ChatClientInterface.hpp"

namespace QodeAssist::Chat {

class ChatWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWidget(QWidget *parent = nullptr);

    void clear();
    void scrollToBottom();
    void setShowTimestamp(bool show);

    void receiveMessage(const QString &message);

private slots:
    void sendMessage();
    void receivePartialMessage(const QString &partialMessage);
    void onMessageCompleted();
    void handleError(const QString &error);

private:
    QTextEdit *m_chatDisplay;
    QLineEdit *m_messageInput;
    QPushButton *m_sendButton;
    bool m_showTimestamp;
    ChatClientInterface *m_chatClient;
    QString m_currentAIResponse;

    void setupUi();
    void addMessage(const QString &message, bool fromUser = true);
    void updateLastAIMessage(const QString &message);
};

} // namespace QodeAssist::Chat
