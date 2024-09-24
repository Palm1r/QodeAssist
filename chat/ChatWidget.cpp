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

#include "ChatWidget.h"
#include "QodeAssistUtils.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QtCore/qtimer.h>

namespace QodeAssist::Chat {

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , m_showTimestamp(false)
    , m_chatClient(new ChatClientInterface(this))
{
    setupUi();

    connect(m_sendButton, &QPushButton::clicked, this, &ChatWidget::sendMessage);
    connect(m_messageInput, &QLineEdit::returnPressed, this, &ChatWidget::sendMessage);

    connect(m_chatClient, &ChatClientInterface::messageReceived, this, &ChatWidget::receiveMessage);
    connect(m_chatClient, &ChatClientInterface::errorOccurred, this, &ChatWidget::handleError);

    logMessage("ChatWidget initialized");
}

void ChatWidget::setupUi()
{
    m_chatDisplay = new QTextEdit(this);
    m_chatDisplay->setReadOnly(true);

    m_messageInput = new QLineEdit(this);
    m_sendButton = new QPushButton("Send", this);

    QHBoxLayout *inputLayout = new QHBoxLayout;
    inputLayout->addWidget(m_messageInput);
    inputLayout->addWidget(m_sendButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_chatDisplay);
    mainLayout->addLayout(inputLayout);

    setLayout(mainLayout);
}

void ChatWidget::sendMessage()
{
    QString message = m_messageInput->text().trimmed();
    if (!message.isEmpty()) {
        logMessage("Sending message: " + message);
        addMessage(message, true);
        m_chatClient->sendMessage(message);
        m_messageInput->clear();
        addMessage("AI is typing...", false);
    }
}

void ChatWidget::receiveMessage(const QString &message)
{
    updateLastAIMessage(message);
}

void ChatWidget::receivePartialMessage(const QString &partialMessage)
{
    logMessage("Received partial message: " + partialMessage);
    m_currentAIResponse += partialMessage;
    updateLastAIMessage(m_currentAIResponse);
}

void ChatWidget::onMessageCompleted()
{
    updateLastAIMessage(m_currentAIResponse);
    m_currentAIResponse.clear();
    scrollToBottom();
}

void ChatWidget::handleError(const QString &error)
{
    logMessage("Error occurred: " + error);
    addMessage("Error: " + error, false);
}

void ChatWidget::addMessage(const QString &message, bool fromUser)
{
    auto prefix = fromUser ? "You: " : "AI: ";
    QString timestamp = m_showTimestamp ? QDateTime::currentDateTime().toString("[hh:mm:ss] ") : "";
    QString fullMessage = timestamp + prefix + message;
    m_chatDisplay->append(fullMessage);
    scrollToBottom();
}

void ChatWidget::updateLastAIMessage(const QString &message)
{
    QTextCursor cursor = m_chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    QString timestamp = m_showTimestamp ? QDateTime::currentDateTime().toString("[hh:mm:ss] ") : "";
    cursor.insertText(timestamp + "AI: " + message);

    cursor.movePosition(QTextCursor::End);
    m_chatDisplay->setTextCursor(cursor);

    scrollToBottom();
    m_chatDisplay->repaint();
}

void ChatWidget::clear()
{
    m_chatDisplay->clear();
    m_currentAIResponse.clear();
    m_chatClient->clearMessages();
}

void ChatWidget::scrollToBottom()
{
    QScrollBar *scrollBar = m_chatDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void ChatWidget::setShowTimestamp(bool show)
{
    m_showTimestamp = show;
}

} // namespace QodeAssist::Chat
