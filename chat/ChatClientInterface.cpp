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

#include "ChatClientInterface.hpp"
#include "LLMProvidersManager.hpp"
#include "PromptTemplateManager.hpp"
#include "QodeAssistUtils.hpp"
#include "settings/ContextSettings.hpp"
#include "settings/GeneralSettings.hpp"
#include "settings/PresetPromptsSettings.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

namespace QodeAssist::Chat {

int ChatHistory::estimateTokenCount(const QString &text) const
{
    return text.length() / 4;
}

void ChatHistory::addMessage(ChatMessage::Role role, const QString &content)
{
    int tokenCount = estimateTokenCount(content);
    m_messages.append({role, content, tokenCount});
    m_totalTokens += tokenCount;
    trim();
}
void ChatHistory::clear()
{
    m_messages.clear();
    m_totalTokens = 0;
}

QVector<ChatMessage> ChatHistory::getMessages() const
{
    return m_messages;
}

QString ChatHistory::getSystemPrompt() const
{
    return m_systemPrompt;
}

void ChatHistory::setSystemPrompt(const QString &prompt)
{
    m_systemPrompt = prompt;
}

void ChatHistory::trim()
{
    while (m_messages.size() > MAX_HISTORY_SIZE || m_totalTokens > MAX_TOKENS) {
        if (!m_messages.isEmpty()) {
            m_totalTokens -= m_messages.first().tokenCount;
            m_messages.removeFirst();
        } else {
            break;
        }
    }
}

ChatClientInterface::ChatClientInterface(QObject *parent)
    : QObject(parent)
    , m_requestHandler(new LLMRequestHandler(this))
{
    connect(m_requestHandler,
            &LLMRequestHandler::completionReceived,
            this,
            [this](const QString &completion, const QJsonObject &, bool isComplete) {
                handleLLMResponse(completion, isComplete);
            });

    connect(m_requestHandler,
            &LLMRequestHandler::requestFinished,
            this,
            [this](const QString &, bool success, const QString &errorString) {
                if (!success) {
                    emit errorOccurred(errorString);
                }
            });

    m_chatHistory.setSystemPrompt("You are a helpful C++ and QML programming assistant.");
}

ChatClientInterface::~ChatClientInterface() = default;

void ChatClientInterface::sendMessage(const QString &message)
{
    logMessage("Sending message: " + message);
    logMessage("chatProvider " + Settings::generalSettings().chatLlmProviders.stringValue());
    logMessage("chatTemplate " + Settings::generalSettings().chatPrompts.stringValue());

    auto chatTemplate = PromptTemplateManager::instance().getCurrentChatTemplate();
    auto chatProvider = LLMProvidersManager::instance().getCurrentChatProvider();

    ContextData context;
    context.prefix = message;
    context.suffix = "";
    if (Settings::contextSettings().useSpecificInstructions())
        context.instriuctions = Settings::contextSettings().specificInstractions();

    QJsonObject providerRequest;
    providerRequest["model"] = Settings::generalSettings().chatModelName();
    providerRequest["stream"] = true;
    providerRequest["messages"] = prepareMessagesForRequest();

    chatTemplate->prepareRequest(providerRequest, context);
    chatProvider->prepareRequest(providerRequest);

    LLMConfig config;
    config.requestType = RequestType::Chat;
    config.provider = chatProvider;
    config.promptTemplate = chatTemplate;
    config.url = QString("%1%2").arg(Settings::generalSettings().chatUrl(),
                                     Settings::generalSettings().chatEndPoint());
    config.providerRequest = providerRequest;

    QJsonObject request;
    request["id"] = QUuid::createUuid().toString();

    m_accumulatedResponse.clear();
    m_chatHistory.addMessage(ChatMessage::Role::User, message);
    m_requestHandler->sendLLMRequest(config, request);
}

void ChatClientInterface::clearMessages()
{
    m_chatHistory.clear();
    m_accumulatedResponse.clear();
    logMessage("Chat history cleared");
}

QVector<ChatMessage> ChatClientInterface::getChatHistory() const
{
    return m_chatHistory.getMessages();
}

void ChatClientInterface::handleLLMResponse(const QString &response, bool isComplete)
{
    m_accumulatedResponse += response;

    if (isComplete) {
        logMessage("Message completed. Final response: " + m_accumulatedResponse);
        emit messageReceived(m_accumulatedResponse.trimmed());

        m_chatHistory.addMessage(ChatMessage::Role::Assistant, m_accumulatedResponse.trimmed());
        m_accumulatedResponse.clear();
    }
}

QJsonArray ChatClientInterface::prepareMessagesForRequest() const
{
    QJsonArray messages;

    messages.append(QJsonObject{{"role", "system"}, {"content", m_chatHistory.getSystemPrompt()}});

    for (const auto &message : m_chatHistory.getMessages()) {
        QString role;
        switch (message.role) {
        case ChatMessage::Role::User:
            role = "user";
            break;
        case ChatMessage::Role::Assistant:
            role = "assistant";
            break;
        default:
            continue;
        }
        messages.append(QJsonObject{{"role", role}, {"content", message.content}});
    }

    return messages;
}

} // namespace QodeAssist::Chat
